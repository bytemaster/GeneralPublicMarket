#ifndef _NODE_HPP_
#define _NODE_HPP_
#include "protocol.hpp"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>


namespace rproxy {
    using namespace boost::asio::ip;

    class control_connection;
    class proxy_connection;


    typedef boost::shared_ptr<std::vector<char> > shared_vec;

    class server {
        
        public:
            server( boost::asio::io_service& i, const boost::filesystem::path& dir );
            ~server();
        
            // used for client proxy
            void listen( uint16_t port );

            // used for server proxy
            void connect_to( const std::string& host_port, const boost::function<void()>& on_connected );
            void map_port( const std::string& local_host_port, uint16_t remote_port );

            tcp::endpoint get_server_endpoint( uint32_t server_id )const { return m_mapped_ports.find(server_id)->second.local_ep; }

        private: 
            friend class control_connection;
            void start_accept();
            void closed( const boost::shared_ptr<control_connection>& c ); 
            void handle_accept( const boost::shared_ptr<control_connection>& con, 
                                const boost::system::error_code& err );

            typedef std::list<boost::shared_ptr<control_connection> >  control_connection_list;
            control_connection_list                                    m_control_connections;

            boost::shared_ptr<control_connection>                      m_out_con;

            boost::asio::io_service&            m_ios;
            boost::shared_ptr<tcp::acceptor>    m_acceptor;

            struct mapped_port
            {
                uint32_t        server_id;
                std::string     host_port;
                tcp::endpoint   local_ep;
            };
            uint32_t                            m_next_server_id;
            std::map<uint32_t,mapped_port>      m_mapped_ports;
    };

} // namespace rpoxy

#endif
