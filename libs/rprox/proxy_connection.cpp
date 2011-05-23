#include "proxy_connection.hpp"
#include "control_connection.hpp"
#include <boost/bind.hpp>

namespace rproxy {

proxy_connection::~proxy_connection()
{
    wlog( "%1%", this );
}

void proxy_connection::read_some()
{
     shared_vec v(new std::vector<char>(2048) ); 
     sock.async_read_some( boost::asio::buffer( *v ),
                                 boost::bind( &proxy_connection::on_read, shared_from_this(), v,
                                              boost::asio::placeholders::error, 
                                              boost::asio::placeholders::bytes_transferred ) );
}

void proxy_connection::on_read( const shared_vec& v, const boost::system::error_code& ec, size_t bytes_read )
{
    if( !ec )
    {
        v->resize(bytes_read);
        boost::shared_ptr<control_connection>(echo_connection)->write( connection_id, v );
        read_some();
        return;
    }
    elog( "proxy connection read error! %1%", boost::system::system_error(ec).what() );
    boost::shared_ptr<control_connection>(echo_connection)->error( connection_id, ec );
}


void proxy_connection::write( const shared_vec& v )
{
    boost::asio::async_write( sock, boost::asio::buffer(*v),
                              boost::bind( &proxy_connection::on_write, shared_from_this(), v,
                                           boost::asio::placeholders::error ) );
}


void proxy_connection::on_write( const shared_vec& data, const boost::system::error_code& ec )
{
    if( ec )
    {
        elog( "proxy connection write error! %1%", boost::system::system_error(ec).what() );
        boost::shared_ptr<control_connection>(echo_connection)->error( connection_id, ec );
    }
}

} // namespace rpoxy
