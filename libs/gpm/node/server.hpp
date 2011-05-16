#ifndef _GPM_SERVER_HPP_
#define _GPM_SERVER_HPP_
#include <gpm/node/node.hpp>
#include <gpm/node/protocol.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

namespace gpm {


struct connection
{
    public:
        connection( boost::asio::io_service& ios, const boost::shared_ptr<gpm::node>& n )
        :sock(ios),m_node(n)
        {
        }

        void start();

        void read_message( int state = 0, const boost::system::error_code& ec = boost::system::error_code(),
                                             size_t bytes_read = 0);
        void handle_message( const gpm::proto::message& msg );

        // called via node->exec()
        void get_full_block(int32_t index);

        // called via node->exec()
        void get_head_block_index();

        void send_transaction( const signed_transaction& trx );
        void send_full_block( const full_block_state& blk );
        void send_message( const proto::message& msg );
        void handle_write( std::vector<char>* data, const boost::system::error_code& err );
    
        void add_full_block( const proto::report_full_block& rfb );
        void add_transaction( const proto::report_transaction& rfb );

        proto::message               msg;
        uint32_t                     msg_size;
        boost::asio::ip::tcp::socket sock;
        boost::shared_ptr<gpm::node> m_node;

        boost::signals::connection new_block_con;
        boost::signals::connection new_trx_con;

        boost::signal<void()>        closed;
};

/**
 *  
 */
class server 
{
    public:
        server( const boost::shared_ptr<gpm::node>& node, uint16_t port )
        :m_node(node), m_acceptor(m_ios, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(),port) )
        {
            slog( "starting node server on port %1%", port );
            the_work = new boost::asio::io_service::work(m_ios);
            m_thread.reset( new boost::thread( boost::bind( &boost::asio::io_service::run, &m_ios ) ) );


            start_accept();
        }
        ~server()
        {
            delete the_work;
            m_ios.stop();
            m_thread->join();
        }
        bool connect_to( const std::string& host_port )
        {
            slog( "attempting to connect to %1%", host_port );
            std::string host = host_port.substr( 0, host_port.find(':') );
            std::string port = host_port.substr( host_port.find(':') + 1, host_port.size() );

            using namespace boost::asio::ip;
            tcp::resolver resolver(m_ios);
            tcp::resolver::query query(host, port );
            tcp::resolver::iterator itr = resolver.resolve(query);
            tcp::resolver::iterator end;
            
            boost::shared_ptr<connection> con( new connection(m_ios, m_node) );
            boost::system::error_code err = boost::asio::error::host_not_found;
            while( err && itr != end )
            {
                con->sock.close();
                con->sock.connect( *itr++, err );
            }
            if( err )
            {
                BOOST_THROW_EXCEPTION( boost::system::system_error(err) );
            }

            slog( "connected to %1%", host_port );
            // TODO: move this to strand
            con->closed.connect( boost::bind( &server::connection_closed, this, con ) );
            m_connections.push_back(con); 
            con->start();
        }


    private:
        void connection_closed( const boost::shared_ptr<connection>& c )
        {
            m_connections.erase( std::find( m_connections.begin(), m_connections.end(), c ) );
        }

        void start_accept()
        {
            boost::shared_ptr<connection> con( new connection(m_ios, m_node) );

            // TODO: wrap in strand to resolve conflict with m_connections...
            m_acceptor.async_accept( con->sock, boost::bind( &server::handle_accept, this, con, boost::asio::placeholders::error ) );
        }

        void handle_accept( const boost::shared_ptr<connection>& con, const boost::system::error_code& error )
        {
            if( !error )
            {
                slog( "new connection" );
                con->closed.connect( boost::bind( &server::connection_closed, this, con ) );
                m_connections.push_back( con );
                con->start();
                start_accept();
            }
            else
            {
                elog( "Error accepting connection" );
            }
        }

        boost::shared_ptr<gpm::node>     m_node;
        boost::asio::io_service          m_ios;
        boost::asio::ip::tcp::acceptor   m_acceptor;
        boost::scoped_ptr<boost::thread> m_thread;

        boost::asio::io_service::work*            the_work;
        std::list<boost::shared_ptr<connection> > m_connections;
};

}

#endif
