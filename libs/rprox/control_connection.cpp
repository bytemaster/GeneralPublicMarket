#include "control_connection.hpp"
#include "proxy_connection.hpp"
#include <boost/bind.hpp>
#include <rprox/rproxy.hpp>
#include <boost/rpc/base64.hpp>

namespace rproxy {

control_connection::~control_connection()
{
    wlog( "%1%", this );
}

void handle_write_complete( const control_connection::ptr& con, std::vector<char>* msg, const boost::system::error_code& ec )
{
    delete msg;
    if( ec )
        elog( boost::system::system_error(ec).what() );
}


void control_connection::send( const proto::message& mr )
{
    std::vector<char>* msg = new std::vector<char>();
    proto::message& m = const_cast<proto::message&>(mr);
    m.data.resize( ((m.data.size()+7)/8)*8 ); 
    boost::rpc::raw::pack( *msg, m );

    if( m_dh.shared_key.size() )
        m_bf.encrypt( (unsigned char*)&msg->front(), msg->size() );

    boost::asio::async_write( sock, boost::asio::buffer(*msg),
                              boost::bind( handle_write_complete, shared_from_this(), msg,
                                           boost::asio::placeholders::error ) );
}

control_connection::control_connection( boost::asio::io_service& i, rproxy::server* n )
:sock(i),m_rproxy_server(n),m_ios(i)
{

}

void control_connection::init_handshake()
{
    m_dh.generate_params( 128, 5 );
    m_dh.generate_pub_key();
    send( proto::init_diffie_hellman( m_dh ) );
}

void control_connection::write( uint32_t con_id, const shared_vec& data )
{
    send( proto::forward_write( con_id, *data ) );
}
void control_connection::error( uint32_t con_id, const boost::system::error_code& ec )
{
    elog( "Error on connection %1%, %2%", con_id, boost::system::system_error(ec).what() );
    send( proto::connection_status( con_id, ec ? 1 : 0, "Error" ) );
}

tcp::socket& control_connection::socket() { return sock; }

void control_connection::read_message( int state, const boost::system::error_code& ec, size_t bytes_read )
{
    if( ec )
    {
        elog( "Error processing message: %1%", boost::system::system_error(ec).what() );
        pcon_map::iterator itr = pcons.begin();
        while( itr != pcons.end() )
        {
            itr->second->sock.close();
            ++itr;
        }
        pcons.clear();
        mapped_ports.clear();
        m_rproxy_server->closed( shared_from_this() );
        return;
    }
    switch( state )
    {
        case 0: // start by reading the id 
           boost::asio::async_read(  sock, boost::asio::buffer( (char*)&msg_id, sizeof(msg_id)+sizeof(msg_size) ),
                                     boost::bind( &control_connection::read_message, this, state + 1, 
                                                  boost::asio::placeholders::error, 
                                                  boost::asio::placeholders::bytes_transferred ) );
            return;
        case 1: // then read the data
            if( m_dh.shared_key.size() )
                m_bf.decrypt((unsigned char*)&msg_id, sizeof(msg_id)+sizeof(msg_size)  );

            msg.id = msg_id;
            msg.data.resize(msg_size);
            boost::asio::async_read( sock, boost::asio::buffer( msg.data ),
                                     boost::bind( &control_connection::read_message, this, state + 1, 
                                                  boost::asio::placeholders::error, 
                                                  boost::asio::placeholders::bytes_transferred ) );

            return;
        case 2: // then process the message
            if( m_dh.shared_key.size() && msg.data.size() )
                m_bf.decrypt((unsigned char*)&msg.data.front(), msg.data.size() );
            handle_message( msg );
               
            // start reading the next message
            read_message();
    }
}
void control_connection::handle_message( const proto::message& msg )
{
   if( msg.id == proto::init_diffie_hellman::id )
   {
        proto::init_diffie_hellman idf = msg;

        // then we have already sent ours
        if( !m_dh.valid )
        {
            m_dh = idf.dh;
            m_dh.generate_pub_key();
            send( proto::init_diffie_hellman( m_dh ) );

            m_dh.compute_shared_key( idf.dh.pub_key );
            wlog( "shared key: %1%", boost::rpc::base64_encode( (const unsigned char*)&m_dh.shared_key.front(), m_dh.shared_key.size() ) );
            m_bf.start( (unsigned char*)&m_dh.shared_key.front(), std::min(m_dh.shared_key.size(),size_t(56) ) );
        }
        else // this is the ack, computing shared key 
        {
            m_dh.compute_shared_key( idf.dh.pub_key );
            wlog( "shared key: %1%", boost::rpc::base64_encode( (const unsigned char*)&m_dh.shared_key.front(), m_dh.shared_key.size() ) );
            m_bf.start( (unsigned char*)&m_dh.shared_key.front(), std::min(m_dh.shared_key.size(),size_t(56) ) );
        }
        connected();
   }
   else if( msg.id == proto::login::id )
   {
        proto::login ca; ca = msg;
        remote_btc_address = ca.btc_address;
   }
   else if( msg.id == proto::create_server::id )
   {
        proto::create_server cs = msg;
        slog( "Request to create server id %2% on port %1%", cs.port, cs.server_id );

        mapped_ports[cs.server_id] = boost::shared_ptr<tcp::acceptor>(new tcp::acceptor(m_ios, tcp::endpoint(tcp::v4(), cs.port) ) );

        boost::shared_ptr<proxy_connection> ncon( new proxy_connection( m_ios, shared_from_this(), ++m_next_con_id ) );
        mapped_ports[cs.server_id]->async_accept( ncon->sock, 
            boost::bind( &control_connection::handle_mapped_port_accept, shared_from_this(), cs.server_id, ncon, boost::asio::placeholders::error ) );
   }
   else if( msg.id == proto::forward_write::id )
   {
        proto::forward_write fw = msg;
        pcons[fw.con_id]->write( shared_vec(new std::vector<char>(fw.data)) );
   }
   else if( msg.id == proto::connection_status::id )
   {
        proto::connection_status cs = msg;
        if( cs.error )
        {
            elog( "Closing connection %1%  %2%", cs.con_id, cs.what );
            std::map<uint32_t, boost::shared_ptr<proxy_connection> >::iterator itr = pcons.find(cs.con_id);
            itr->second->sock.close();
            pcons.erase( itr );
        }
   }
   else if( msg.id == proto::create_connection::id )
   {
        proto::create_connection cc = msg;
        pcons[cc.con_id] = proxy_connection::ptr( new proxy_connection( m_ios, shared_from_this(), cc.con_id ) );

        boost::system::error_code error = boost::asio::error::host_not_found;
        pcons[cc.con_id]->sock.connect( m_rproxy_server->get_server_endpoint( cc.server_id ), error );

        if( error )
        {
            elog( "%1%", boost::system::system_error(error).what() );
            return;
        }
        pcons[cc.con_id]->read_some();
   }
   else
        elog( "Unknown message id %1%", int(msg.id) );
}


/**
 *  
 *
 */
void control_connection::handle_mapped_port_accept( uint32_t sid, 
                                                    const boost::shared_ptr<proxy_connection>& pcon,
                                                    const boost::system::error_code& err )
{
    slog( "Service id %1% received connection, forwarding", sid );
    if( !err )
    {
        send( proto::create_connection( sid, pcon->connection_id ) );
        pcons[pcon->connection_id] = pcon;
        wlog( "pcons.size: %1%", pcons.size() );
        pcon->read_some();

        boost::shared_ptr<proxy_connection> ncon( new proxy_connection( m_ios, shared_from_this(), ++m_next_con_id ) );
        mapped_ports[sid]->async_accept( ncon->sock, 
            boost::bind( &control_connection::handle_mapped_port_accept, shared_from_this(), sid, ncon, boost::asio::placeholders::error ) );
    }
    else
    {
        elog( boost::system::system_error(err).what() );
    }
}

} // namespace rproxy

