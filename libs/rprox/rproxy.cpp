#include "rproxy.hpp"
#include "protocol.hpp"
#include <bitcoin/bitcoin.hpp>

#include <fstream>

#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include <gpm/crypto/blowfish.hpp>
#include <boost/rpc/super_fast_hash.hpp>
#include <boost/rpc/base64.hpp>

#include "proxy_connection.hpp"
#include "control_connection.hpp"

namespace rproxy {

server::server( boost::asio::io_service& i, const boost::filesystem::path& data_dir )
:m_ios(i)
{
}
server::~server()
{
}

void server::connect_to( const std::string& host_port, const boost::function<void()>& on_connected )
{
    slog( "attempting to connect to %1%", host_port );
    std::string host = host_port.substr( 0, host_port.find(':') );
    std::string port = host_port.substr( host.size() + 1 );

    tcp::resolver resolver(m_ios);
    tcp::resolver::query q(host,port);
    tcp::resolver::iterator epi = resolver.resolve(q);
    tcp::resolver::iterator end;

    control_connection::ptr con(new control_connection(m_ios, this));

    boost::system::error_code error = boost::asio::error::host_not_found;
    while( error && epi != end )
    {
        con->socket().close();
        con->socket().connect( *epi, error );
    }
    if( error )
    {
        elog( "%1%", boost::system::system_error(error).what() );
        return;
    }
        
    con->connected.connect(on_connected);
    con->init_handshake();
    con->start();
    m_out_con = con;


    slog( "connected to %1%", host_port );
}

void server::map_port( const std::string& host_port, uint16_t rport )
{
    slog( "attempting to connect to %1%", host_port );
    std::string host = host_port.substr( 0, host_port.find(':') );
    std::string port = host_port.substr( host.size() + 1 );

    tcp::resolver resolver(m_ios);
    tcp::resolver::query q(host,port);
    tcp::resolver::iterator epi = resolver.resolve(q);
    tcp::resolver::iterator end;

    tcp::socket test_sock(m_ios);

    mapped_port mp; mp.host_port = host_port;

    boost::system::error_code error = boost::asio::error::host_not_found;
    while( error && epi != end )
    {
        test_sock.close();
        test_sock.connect( *epi, error );
    }
    if( error )
    {
        elog( "%1%", boost::system::system_error(error).what() );
        return;
    }
    mp.local_ep  = *epi;
    mp.server_id = ++m_next_server_id;     
    slog( "Sending request to map remote %2%:%3% to %1%", host_port, m_out_con->socket().remote_endpoint().address(), rport );

    m_mapped_ports[mp.server_id] =  mp;
    
    m_out_con->send( proto::create_server( mp.server_id, rport ) );
}


void server::listen( uint16_t port )
{
    slog( "listening on port %1%", port );
    m_acceptor = boost::shared_ptr<tcp::acceptor>(new tcp::acceptor( m_ios, tcp::endpoint( tcp::v4(), port ) ) );
    start_accept();
}

void server::start_accept()
{
    control_connection::ptr c( new control_connection( m_ios, this ) );
    m_acceptor->async_accept( c->socket(), boost::bind( &server::handle_accept, this, c, boost::asio::placeholders::error ) );
}

void server::closed( const control_connection::ptr& c )
{
    control_connection_list::iterator itr =  std::find( m_control_connections.begin(), m_control_connections.end(), c );
    if( itr != m_control_connections.end() )
    {
        wlog( "removing from connections" );
        m_control_connections.erase( itr );
    }
    else if( c == m_out_con ) 
    {
        wlog( "Lost connection to service" );
        m_out_con.reset();
    }
    else
        elog( "Unable to find control connection!" );

}


// CALLED FROM ASIO... TODO: make thread safe
void server::handle_accept( const control_connection::ptr& c, const boost::system::error_code& err )
{
    wlog( "new control_connection %1%", c.get() ); 
    if( !err ) {
        m_control_connections.push_back(c);
        c->start();
        start_accept();
    }
    else
    {
        elog( boost::system::system_error(err).what() );
    }
}


}
