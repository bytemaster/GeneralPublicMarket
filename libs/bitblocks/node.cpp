#include "node.hpp"
#include "block.hpp"
#include "protocol.hpp"
#include "bitcoin.hpp"

#include <fstream>

#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include <gpm/crypto/blowfish.hpp>
#include <boost/rpc/super_fast_hash.hpp>
#include <boost/rpc/base64.hpp>

node::node( boost::asio::io_service& i, const boost::filesystem::path& data_dir )
:m_ios(i)
{
    if( !boost::filesystem::exists( data_dir ) )
        boost::filesystem::create_directory( data_dir );
    m_blocks.open( (data_dir / "blocks.db").string() );
}


connection::ptr node::connect_to( const std::string& host_port )
{
    slog( "attempting to connect to %1%", host_port );
    std::string host = host_port.substr( 0, host_port.find(':') );
    std::string port = host_port.substr( host.size() + 1 );

    tcp::resolver resolver(m_ios);
    tcp::resolver::query q(host,port);
    tcp::resolver::iterator epi = resolver.resolve(q);
    tcp::resolver::iterator end;

    connection::ptr con(new connection(m_ios, this));

    boost::system::error_code error = boost::asio::error::host_not_found;
    while( error && epi != end )
    {
        con->socket().close();
        con->socket().connect( *epi, error );
    }
    if( error )
    {
        elog( "%1%", boost::system::system_error(error).what() );
        return connection::ptr();
    }
        
    con->start();
    m_connections.push_back(con);

    slog( "connected to %1%", host_port );
    return con;
}

void handle_write_complete( const connection::ptr& con, std::vector<char>* msg, const closure& c, const boost::system::error_code& ec )
{
    delete msg;
    if( c ) 
        c(ec);
    else if( ec )
        elog( boost::system::system_error(ec).what() );
}

void connection::send( const proto::message& m, const closure& c )
{
    slog( "sending message..." );
    std::vector<char>* msg = new std::vector<char>();
    boost::rpc::raw::pack( *msg, m );

    boost::asio::async_write( sock, boost::asio::buffer(*msg),
                              boost::bind( handle_write_complete, shared_from_this(), msg, c,
                                           boost::asio::placeholders::error ) );
}


void node::store_block( const sha1_hashcode& hc, const std::vector<char>& d, const std::string& payto )
{
    block b;
    b.payto_address = payto;
    b.data = d;

    m_blocks.set( hc, b );
}

/**
 *   Generate a blowfish key, encrypt each 1MB block and
 *   store it in the database under the hash of the block.
 *
 *   Add each hash + file name to the metablock.
 *
 *   Then encrypt the metablock, hash it and store it.
 *
 *   Return the base64 hash of the meta block + the blowfish key.
 *
 */
std::string node::import_file( const boost::filesystem::path& filename )
{
    if( !boost::filesystem::exists( filename ) )
        return std::string("file does not exist");


    std::stringstream ss; ss << boost::rpc::super_fast_hash( filename.string() );
    std::string bf_key = ss.str();

    gpm::blowfish bf;
    bf.start( (unsigned char*)bf_key.c_str(), bf_key.size() );

    metafile  mf( boost::filesystem::path(filename).filename() );

    block b;
    b.data.resize(1024*1024);

    std::ifstream in;
    in.open( filename.string().c_str(), std::ios::binary );
    in.seekg( 0, std::ios::end );
    size_t len = in.tellg();
    mf.size = len;
    in.seekg( 0, std::ios::beg );

    while( len > 0 )
    {
        b.data.resize( std::min( size_t(1024*1024), size_t((len+7)/8)*8 ) );
        in.read( &b.data.front(), b.data.size() );
        bf.encrypt( (unsigned char*)&b.data.front(), b.data.size() );
        bf.reset_chain();

        mf.blocks.push_back( boost::rpc::hash_sha1( &b.data.front(), b.data.size() ) );

        m_blocks.set( mf.blocks.back(), b );

        len -= b.data.size();
    }

    // pack the metafile
    boost::rpc::raw::pack( b.data, mf );
    b.data.resize( ((b.data.size()+7)/8) * 8 );
    // encrypt the metafile
    bf.encrypt( (unsigned char*)&b.data.front(), b.data.size() );

    // hash the meta file 
    sha1_hashcode hc = boost::rpc::hash_sha1( &b.data.front(), b.data.size() );

    // store the metafile
    m_blocks.set( hc, b );
    m_blocks.sync();


    return bf_key + "_" + boost::rpc::base64_encode( (unsigned char*)hc.hash, sizeof(hc.hash) );
}


/// data is freed when upload is complete
void node::upload_block( const std::string& payto, const std::vector<char>& data, const closure& c )
{
    if( !m_connections.size() ) 
        THROW_GPM_EXCEPTION( "No connections" );

    m_connections.front()->send( proto::store_block( data, payto ), c );
}


/**
 *  Uploading a file can be a long-running task, this tracks the state loading and
 *  uploading parts as necessary.  Keep it async!
 */
struct upload_file_state
{
    upload_file_state( const std::string& bk, const boost::filesystem::path& filename, const std::string& pt, const node::upload_file_closure& c, node* n )
    :mf(filename.string()),bf_key(bk),payto(pt),m_closure(c),m_node(n)
    {
        bf.start( (unsigned char*)bf_key.c_str(), bf_key.size() );
        cur_block.data.resize( 1024*1024 );

        mf.name = boost::filesystem::path(filename).filename();

        in.open( filename.string().c_str(), std::ios::binary );
        in.seekg( 0, std::ios::end );
        len = in.tellg();
        mf.size = len;
        in.seekg( 0, std::ios::beg );

        send_block(boost::system::error_code());
    }

    void send_block( const boost::system::error_code& ec )
    {
        slog( "sending block... %1% bytes remaining", len );
        if( ec )
        {
            if( m_closure ) 
                m_closure("Error",ec); 
            delete this;
            return;
        }
        if( len > 0 )
        {
            cur_block.data.resize( std::min( size_t(1024*1024), size_t((len+7)/8)*8 ) );
            in.read( &cur_block.data.front(), cur_block.data.size() );
            bf.encrypt( (unsigned char*)&cur_block.data.front(), cur_block.data.size() );
            bf.reset_chain();

            mf.blocks.push_back( boost::rpc::hash_sha1( &cur_block.data.front(), cur_block.data.size() ) );

            len -= cur_block.data.size();
            slog( "upload block" );
            // send..
            m_node->upload_block( payto, cur_block.data, boost::bind( &upload_file_state::send_block, this, _1 ) );
        }
        else
        {
            // pack the metafile
            boost::rpc::raw::pack( cur_block.data, mf );
            cur_block.data.resize( ((cur_block.data.size()+7)/8) * 8 );
            // encrypt the metafile
            bf.encrypt( (unsigned char*)&cur_block.data.front(), cur_block.data.size() );

            // hash the meta file 
            sha1_hashcode hc = boost::rpc::hash_sha1( &cur_block.data.front(), cur_block.data.size() );
            std::string urn = bf_key + "_" + boost::rpc::base64_encode( (unsigned char*)hc.hash, sizeof(hc.hash) );
            m_node->upload_block( payto, cur_block.data, boost::bind( &upload_file_state::cleanup, this, _1, urn ) );
        }
    }

    void cleanup( const boost::system::error_code& ec, const std::string& urn ) 
    { 
        slog( "done sending file" );
        if( m_closure ) 
            m_closure(urn,ec); 
        delete this; 
    }

    size_t        len;
    node*         m_node;
    metafile      mf;
    std::string   bf_key;
    gpm::blowfish bf;
    block         cur_block;
    std::string   payto;
    node::upload_file_closure m_closure;


    std::ifstream in;
};

void node::upload_file( const std::string& payto, const boost::filesystem::path& filename, 
                        const node::upload_file_closure& c )
{
    if( !boost::filesystem::exists( filename ) )
        THROW_GPM_EXCEPTION( "file '%1%' does not exist", %filename );

    std::stringstream ss; ss << boost::rpc::super_fast_hash( filename.string() );
    std::string bf_key = ss.str();

    new upload_file_state( bf_key, filename, payto, c, this );
}


struct download_urn_state
{
    std::string bf_key;
    std::string meta;
    std::string hash;
    sha1_hashcode hc;
    closure       c;
    node*         n;
    gpm::blowfish bf;
    boost::scoped_ptr<std::ofstream> ofile;
    metafile      mf;
    boost::filesystem::path dir;

    std::vector<char> blk_data;

    download_urn_state( const boost::filesystem::path& _dir, const std::string& key_meta, const closure& _c, node* _n )
    :c(_c),n(_n),dir(_dir)
    {
        slog( "start download" );
        int sep = key_meta.find( '_' );
        bf_key  = key_meta.substr(0,sep);
        meta    = key_meta.substr(sep+1);
        hash    = boost::rpc::base64_decode(meta);
        memcpy( hc.hash, hash.c_str(), hash.size() );

        if( !boost::filesystem::exists( dir ) )
            boost::filesystem::create_directory( dir );

        n->download_block( hc, blk_data, boost::bind( &download_urn_state::meta_block_complete, this, _1 ) );
    }

    void meta_block_complete( const boost::system::error_code& ec )
    {
        if( !ec )
        {
            bf.start( (unsigned char*)bf_key.c_str(), bf_key.size() );
            bf.decrypt( (unsigned char*)&blk_data.front(), blk_data.size() );
            bf.reset_chain();

            boost::rpc::raw::unpack( blk_data, mf );

            slog( "Exporting file '%1%' of size '%2%' kb", mf.name, mf.size/1024 );

            ofile.reset( new std::ofstream( (dir / mf.name).string().c_str() ));
            if( mf.blocks.size() )
            {
                n->download_block( mf.blocks[0], blk_data, boost::bind( &download_urn_state::block_complete, this, 0, _1 ) );
            }
            else {
                if( c ) c(ec);

                delete this;
            }
        }
        else
        {
            c(ec);
            delete this;
        }
    }
    void block_complete( int i, const boost::system::error_code& ec )
    {
        slog( "block complete %1%", i );
        if( ec )
        {
            if(c) 
                c(ec);
            delete this;
            return;
        }
        if( blk_data.size() )
        {
            bf.decrypt( (unsigned char*)&blk_data.front(), blk_data.size() );
            bf.reset_chain();
            ofile->write( &blk_data.front(), blk_data.size() );
        }
        if( i+1 < mf.blocks.size() )
        {
           n->download_block( mf.blocks[i+1], blk_data, boost::bind( &download_urn_state::block_complete, this, i+1, _1 ) );
        }
        else    
        {
            if(c)
                c(ec);
            delete this;
        }
    }
};

void node::download_block( const sha1_hashcode& hc, std::vector<char>& dest, const closure& c )
{
    m_connections.front()->request_block( hc, dest, 0, -1, c );
}

void node::download_urn( const std::string& urn, const boost::filesystem::path& dir, const closure& c )
{
    new download_urn_state( dir, urn, c, this );
}

/**
 *  split the key / hash
 *  grab the block for the hash
 *  decrypt it with the key
 *  unpack the metafile
 *  create a new file with name
 *  for each block, grab it, decrypt it, write it
 */
void node::export_file( const std::string& key_meta, const boost::filesystem::path& dir  )
{
    int sep = key_meta.find( '_' );
    std::string bf_key  = key_meta.substr(0,sep);
    std::string meta    = key_meta.substr(sep+1);
    std::string hash    = boost::rpc::base64_decode(meta);
    sha1_hashcode hc; memcpy( hc.hash, hash.c_str(), hash.size() );

    block b; 
    if( !m_blocks.get( hc, b ) )
    {
        elog( "Unable to find meta block '%1%'", meta );
    }

    if( !boost::filesystem::exists( dir ) )
        boost::filesystem::create_directory( dir );

    gpm::blowfish bf;
    bf.start( (unsigned char*)bf_key.c_str(), bf_key.size() );
    bf.decrypt( (unsigned char*)&b.data.front(), b.data.size() );
    bf.reset_chain();

    metafile mf;
    boost::rpc::raw::unpack( b.data, mf );

    slog( "Exporting file '%1%' of size '%2%' kb", mf.name, mf.size/1024 );

    std::ofstream ofile( (dir / mf.name).string().c_str() );

    uint64_t len = 0;
    for( uint32_t i = 0; i < mf.blocks.size(); ++i )
    {
        if( !m_blocks.get( mf.blocks[i], b ) )
        {
            elog( "Unable to find block %1% of %2%, %3%", (i+1), mf.blocks.size(), mf.blocks[i] );
        }
        bf.decrypt( (unsigned char*)&b.data.front(), b.data.size() );
        bf.reset_chain();
        uint64_t nlen = len + b.data.size();
        if( nlen > mf.size ) nlen = mf.size;
        ofile.write( &b.data.front(), nlen - len );
        len = nlen;
    }

    slog( "Done exporting %1%/%2%", dir, mf.name );
}

void node::read_block( const sha1_hashcode& hc, std::vector<char>& d, uint64_t start, uint64_t len  )
{
    block b; 
    if( !m_blocks.get( hc, b ) )
    {
        elog( "Unable to find block %1%", hc );
    }
    d = b.data;
}

void node::listen( uint16_t port )
{
    slog( "listening on port %1%", port );
    m_acceptor = boost::shared_ptr<tcp::acceptor>(new tcp::acceptor( m_ios, tcp::endpoint( tcp::v4(), port ) ) );
    start_accept();
}

void node::start_accept()
{
    connection::ptr c( new connection( m_ios, this ) );
    m_acceptor->async_accept( c->socket(), boost::bind( &node::handle_accept, this, c, boost::asio::placeholders::error ) );
}

void node::closed( const connection::ptr& c )
{
    m_connections.erase( std::find( m_connections.begin(), m_connections.end(), c ) );
}


// CALLED FROM ASIO... TODO: make thread safe
void node::handle_accept( const connection::ptr& c, const boost::system::error_code& err )
{
    slog( "new connection" ); 
    if( !err ) {
        m_connections.push_back(c);
        c->start();
        start_accept();
    }
    else
    {
        elog( boost::system::system_error(err).what() );
    }
}



connection::connection( boost::asio::io_service& i, node* n )
:sock(i),m_node(n)
{

}

tcp::socket& connection::socket() { return sock; }

void connection::read_message( int state, const boost::system::error_code& ec, size_t bytes_read )
{
    //slog( "state: %1% read %2%", state, bytes_read );
    if( ec )
    {
        elog( "Error processing message: %1%", boost::system::system_error(ec).what() );
        m_node->closed( shared_from_this() );
        return;
    }
    switch( state )
    {
        case 0: // start by reading the id 
           boost::asio::async_read(  sock, boost::asio::buffer( (char*)&msg.id, sizeof(msg.id) ),
                                     boost::bind( &connection::read_message, this, state + 1, 
                                                  boost::asio::placeholders::error, 
                                                  boost::asio::placeholders::bytes_transferred ) );
           return;
        case 1: // then read the size
            boost::asio::async_read( sock, boost::asio::buffer( (char*)&msg_size, sizeof(msg_size) ),
                                     boost::bind( &connection::read_message, this, state + 1, 
                                                  boost::asio::placeholders::error, 
                                                  boost::asio::placeholders::bytes_transferred ) );

            return;
        case 2: // then read the data
            msg.data.resize(msg_size);
            boost::asio::async_read( sock, boost::asio::buffer( msg.data ),
                                     boost::bind( &connection::read_message, this, state + 1, 
                                                  boost::asio::placeholders::error, 
                                                  boost::asio::placeholders::bytes_transferred ) );

            return;
        case 3: // then process the message
            handle_message( msg );
               
            // start reading the next message
            read_message();

    }
}
void connection::handle_message( const proto::message& msg )
{
    slog( "received message" );
   if( msg.id == proto::create_account::id )
   {
        proto::create_account ca; ca = msg;
        remote_btc_address = ca.btc_address;
   }
   else if( msg.id == proto::store_block::id )
   {
        proto::store_block sb; sb = msg;
        sha1_hashcode hc(boost::rpc::hash_sha1( &sb.data.front(), sb.data.size()));
        slog( "storing block of size %1%", sb.data.size() );
        m_node->store_block( hc, sb.data, sb.payto_address );
   }
   else if( msg.id == proto::request_block::id )
   {
        proto::request_block req_block; req_block = msg;
        // read the block
        proto::report_block rb(req_block.request_id, req_block.start );
        m_node->read_block( req_block.block_id, rb.data, req_block.start, req_block.len );
        // send response
        send( rb );
   }
   else if( msg.id == proto::report_block::id )
   {
        proto::report_block rb; rb = msg;

        // find the closure
        preq_map::iterator itr = m_requests.find( rb.request_id );
        if( itr != m_requests.end() )
        {
            itr->second->data = rb.data;
            itr->second->close(boost::system::error_code());
            m_requests.erase(itr);
        }
        else
        {
            elog( "Unexpected request id: %1%", rb.request_id );
        }
   }
   else
        elog( "Unknown message id %1%", int(msg.id) );
}

void connection::request_block( const sha1_hashcode& hc, std::vector<char>& data, uint64_t start, uint64_t len, const closure& c )
{
    pending_request::ptr preq( new pending_request( data, c ));
    m_requests[++next_req_id] = preq;
    send( proto::request_block( next_req_id, hc, start, len ) );
}
