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

boost::asio::io_service::work*  the_work = 0;
block_db&                       get_blocks_db()    { static block_db blocks; return blocks; }
boost::asio::io_service&        main_ios()         { static boost::asio::io_service ioserv; return ioserv; }

class client;
client*                         gclient = 0;

std::string import_file( const std::string& filename );
void export_file( const std::string& key_hash );

using namespace boost::asio::ip;

class client {

    public:
    client( const udp::endpoint& ep, boost::asio::io_service& i = main_ios() )
    :sock( i, udp::endpoint( udp::v4(), 0 ) ), recv_buffer(2048), server_ep(ep)
    {
        start_receive();
    }

    void send_create_account()
    {
        // bitcoin->create_new_address()

        send( create_account( "recv_btc_addr" ) );
    }

    void send( const message& msg )
    {
        std::vector<char>* v = new std::vector<char>();
        boost::rpc::raw::pack( *v, msg );
        sock.async_send_to( boost::asio::buffer( *v ), server_ep,
                            boost::bind( &client::handle_sendto, this, v, 
                                         boost::asio::placeholders::error, 
                                         boost::asio::placeholders::bytes_transferred ) );
    }

    private:
    void start_receive()
    {
        sock.async_receive_from(  boost::asio::buffer( recv_buffer ), remote_ep,
                                  boost::bind( &client::handle_receive, this, boost::asio::placeholders::error,
                                                                              boost::asio::placeholders::bytes_transferred ) );
    }

    void handle_sendto( std::vector<char>* v, const boost::system::error_code& error, std::size_t transfered )
    {
        if( error )
            elog( "%1%", boost::system::system_error(error).what() );
        delete v;
    }

    void handle_receive( const boost::system::error_code& error, std::size_t transfered )
    {
        if( !error || error == boost::asio::error::message_size )
        {
            message msg;
            boost::rpc::raw::unpack( recv_buffer, msg );
            switch( msg.id )
            {
                case create_account::id:       handle_create_account( msg, remote_ep );      break;
                case account_created::id:      handle_close_account( msg, remote_ep );       break;
                case query_server_status::id:  handle_query_server_status( msg, remote_ep ); break;
                case request_bitblock::id:     handle_request_bitblock( msg, remote_ep ); break;
            }
        }
        else
        {
            elog( "%1%", boost::system::system_error(error).what() );
        }
        start_receive();
    }

    void handle_create_account( const create_account& msg, const udp::endpoint& ep  )
    {
        slog( "Create account %1%  recv btc address '%2%'", ep, msg.recv_btc_address );
        // check to see if current account exists for user
        // if recv address is still the same, return same payto address
        // else transfer balance to old recv from address, create new payto address, reset balances
    }
    void handle_close_account( const close_account& msg, const udp::endpoint& ep  )
    {
        slog( "Close account %1%", ep );

        // transfer balance to recv from
    }

    void handle_query_server_status( const query_server_status& msg, const udp::endpoint& ep  )
    {
        slog( "Query server status %1%", ep );
    }

    void handle_request_bitblock( const request_bitblock& rbb, const udp::endpoint& ep )
    {
    }

    private:
        udp::socket         sock;
        udp::endpoint       remote_ep; 
        udp::endpoint       server_ep; 
        std::vector<char>   recv_buffer;
};

void handle_line( const std::string& line )
{
    slog( "%1%", line );

    std::stringstream ss(line);
    std::string cmd;
    ss >> cmd;

    if( cmd == "help" )
    {
        std::cout<< "import infile      - loads a file and outputs the URN\n";
        std::cout<< "export URN         - exports the file previously imported\n";
        std::cout<< "upload URN         - sends the file to the server\n";
        std::cout<< "download URN       - downloads the file from the server\n";
        std::cout<< "exit               - close account and disconnect\n";
    }
    else if( cmd == "import" )
    {
        std::string file;
        ss >> file;

        std::string urn = import_file( file );
        std::cout << "URN: " << urn << std::endl;
    }
    else if( cmd == "export" )
    {
        std::string urn;
        ss >> urn;
        export_file( urn );
    }
    else if( cmd == "upload" )
    {
        std::string urn;
        ss >> urn;
        //upload_file( urn );
    }
    else
        std::cout << "unknown command '"<<cmd<<"'\n";


    std::cout<<"$]";
}

void handle_cli()
{
    std::cout<<"$]";
    std::string line;
    while( line != "exit" ) {
        std::getline(std::cin,line);
        main_ios().post( boost::bind( handle_line, line ) );
    }
    delete the_work;
    main_ios().stop();
}

int main( int argc, char** argv )
{
    try {
        std::string server_host_port;
        std::string data_dir;

        namespace po = boost::program_options;
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help,h", "print this help message." )
            ("server,s", po::value<std::string>(&server_host_port)->default_value("localhost:8000"), "Port to run server on." )
            ("data_dir,d", po::value<std::string>(&data_dir)->default_value("client_bitblocks"), "Directory to store data" )
        ;

        po::variables_map vm;
        po::store( po::parse_command_line(argc,argv,desc), vm );
        po::notify(vm);

        if( vm.count("help") )
        {
            std::cout << desc << std::endl;
            return 0;
        }
        if( !boost::filesystem::exists( data_dir ) )
            boost::filesystem::create_directory( data_dir );

        boost::thread cli_thread(handle_cli);

        get_blocks_db().open( data_dir + "/blocks.db" ); 

        // lookup server
       
        udp::resolver resolver(main_ios());
        size_t pos = server_host_port.find( ':' );
        udp::resolver::query q(udp::v4(), server_host_port.substr(0, pos), server_host_port.substr(pos+1) );

        client* c = new client( *resolver.resolve(q)  );
        c->send_create_account();
        gclient = c;

        the_work = new boost::asio::io_service::work(main_ios());
        main_ios().run();

    } catch ( const boost::exception& e )
    {
        elog( "%1%", boost::diagnostic_information(e) );
    }
    return 0;
}



/**
 *  Start an async process to upload a URN
 */
void upload_file( const std::string& key_meta )
{
    int sep = key_meta.find( '_' );
    std::string bf_key  = key_meta.substr(0,sep);
    std::string meta    = key_meta.substr(sep+1);
    std::string hash    = boost::rpc::base64_decode(meta);
    sha1_hashcode hc; memcpy( hc.hash, hash.c_str(), hash.size() );

    block b; 
    if( !get_blocks_db().get( hc, b ) )
    {
        elog( "Unable to find meta block '%1%'", meta );
    }

    gpm::blowfish bf;
    bf.start( (unsigned char*)bf_key.c_str(), bf_key.size() );
    bf.decrypt( (unsigned char*)&b.data.front(), b.data.size() );
    bf.reset_chain();

    boost::shared_ptr<metafile>  mf(new metafile());
    boost::rpc::raw::unpack( b.data, *mf );

    slog( "Uploading file '%1%' of size '%2%' kb", mf->name, mf->size/1024 );

    
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
std::string import_file( const std::string& filename )
{
    if( !boost::filesystem::exists( filename ) )
        return std::string("file does not exist");


    std::stringstream ss; ss << boost::rpc::super_fast_hash( filename );
    std::string bf_key = ss.str();

    gpm::blowfish bf;
    bf.start( (unsigned char*)bf_key.c_str(), bf_key.size() );

    metafile  mf( boost::filesystem::path(filename).filename() );

    block b;
    b.data.resize(1024*1024);

    std::ifstream in;
    in.open( filename.c_str(), std::ios::binary );
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

        get_blocks_db().set( mf.blocks.back(), b );

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
    get_blocks_db().set( hc, b );
    get_blocks_db().sync();


    return bf_key + "_" + boost::rpc::base64_encode( (unsigned char*)hc.hash, sizeof(hc.hash) );
}

/**
 *  split the key / hash
 *  grab the block for the hash
 *  decrypt it with the key
 *  unpack the metafile
 *  create a new file with name
 *  for each block, grab it, decrypt it, write it
 */
void export_file( const std::string& key_meta  )
{
    int sep = key_meta.find( '_' );
    std::string bf_key  = key_meta.substr(0,sep);
    std::string meta    = key_meta.substr(sep+1);
    std::string hash    = boost::rpc::base64_decode(meta);
    sha1_hashcode hc; memcpy( hc.hash, hash.c_str(), hash.size() );

    block b; 
    if( !get_blocks_db().get( hc, b ) )
    {
        elog( "Unable to find meta block '%1%'", meta );
    }

    gpm::blowfish bf;
    bf.start( (unsigned char*)bf_key.c_str(), bf_key.size() );
    bf.decrypt( (unsigned char*)&b.data.front(), b.data.size() );
    bf.reset_chain();

    metafile mf;
    boost::rpc::raw::unpack( b.data, mf );

    slog( "Exporting file '%1%' of size '%2%' kb", mf.name, mf.size/1024 );

    std::ofstream ofile( mf.name.c_str() );

    uint64_t len = 0;
    for( uint32_t i = 0; i < mf.blocks.size(); ++i )
    {
        if( !get_blocks_db().get( mf.blocks[i], b ) )
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
}

