#ifndef _RPROXY_PROXY_CONNECTION_HPP_
#define _RPROXY_PROXY_CONNECTION_HPP_
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/weak_ptr.hpp>

namespace rproxy {
    using namespace boost::asio::ip;

    typedef boost::shared_ptr<std::vector<char> > shared_vec;

    class control_connection;

    /*
     *  When data is read, it gets sent to echo_connection->write( connection_id, data )
     */
    class proxy_connection : public boost::enable_shared_from_this<proxy_connection>
    {
        public:
            typedef boost::shared_ptr<proxy_connection> ptr;
            proxy_connection( boost::asio::io_service& ios, const boost::weak_ptr<control_connection>& c, uint32_t id  )
            :sock(ios),echo_connection(c),connection_id(id){  }

            ~proxy_connection();

            void                  read_some();
            void                  on_read( const shared_vec& data, const boost::system::error_code& ec, size_t bytes_read );

            void                  write( const shared_vec&  data );
            void                  on_write( const shared_vec& data, const boost::system::error_code& ec );

            uint32_t              connection_id;
            tcp::socket           sock;
            boost::weak_ptr<control_connection>   echo_connection;
    };

} // namespace 

#endif
