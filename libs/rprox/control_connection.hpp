#ifndef RPROXY_CONTROL_CONNECTION_HPP_
#define RPROXY_CONTROL_CONNECTION_HPP_
#include <boost/enable_shared_from_this.hpp>
#include <rprox/protocol.hpp>
#include <boost/asio.hpp>
#include <map>
#include <boost/signals.hpp>

#include <gpm/crypto/blowfish.hpp>


namespace rproxy {

class server;
class proxy_connection;

typedef boost::shared_ptr<std::vector<char> > shared_vec;

using namespace boost::asio::ip;

class control_connection : public boost::enable_shared_from_this<control_connection>
{
    public:
        typedef boost::shared_ptr<control_connection> ptr;

        control_connection( boost::asio::io_service& _ios, rproxy::server* n );
        ~control_connection();

        void init_handshake();
        void start() { read_message(); }
        void send( const proto::message& m );

        tcp::socket& socket();

        void error( uint32_t con_id, const boost::system::error_code& ec );
        void write( uint32_t con_id, const shared_vec& data );

        boost::signal<void()> connected;

    private:
        uint32_t          msg_id;
        uint32_t          msg_size;

        void read_message( int state = 0, const boost::system::error_code& ec = boost::system::error_code(),
                                             size_t bytes_read = 0);
        void handle_message( const proto::message& msg );

        typedef uint32_t connection_id;
        typedef std::map<connection_id,boost::shared_ptr<proxy_connection> > con_map;

        // address to send refunds to on disconnect
        std::string       remote_btc_address;

        // address created just for this control_connection, 
        std::string       local_btc_address;
        uint64_t          total_recv_btc;  //  btc received at local_btc_address
        uint64_t          total_spent_btc; //  amount spent on downloads
        
        proto::message    msg;
        rproxy::server*   m_rproxy_server;
        tcp::socket       sock;


        void handle_mapped_port_accept( uint32_t sid, 
                                        const boost::shared_ptr<proxy_connection>& pcon,
                                        const boost::system::error_code& err );

        typedef std::map<uint32_t, boost::shared_ptr<proxy_connection> > pcon_map;
        pcon_map pcons;

        // server id to acceptor
        std::map<uint32_t, boost::shared_ptr<tcp::acceptor> > mapped_ports;

        uint32_t m_next_con_id;
        boost::asio::io_service& m_ios;

        gpm::diffie_hellman m_dh;
        gpm::blowfish       m_bf;
};

} // namespace rproxy


#endif // RPROXY_CONTROL_CONNECTION
