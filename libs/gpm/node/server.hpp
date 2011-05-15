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

        void read_message( int state = 0, const boost::system::error_code& ec = boost::system::error_code(),
                                             size_t bytes_read = 0)
        {
            if( ec )
            {
                elog( "Error processing message." );
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
                    boost::asio::async_read(  sock, boost::asio::buffer( (char*)&msg_size, sizeof(msg_size) ),
                                             boost::bind( &connection::read_message, this, state + 1, 
                                                          boost::asio::placeholders::error, 
                                                          boost::asio::placeholders::bytes_transferred ) );

                    return;
                case 2: // then read the data
                    msg.data.resize(msg_size);
                    boost::asio::async_read(  sock, boost::asio::buffer( msg.data ),
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
        void handle_message( const gpm::proto::message& msg );

        // called via node->exec()
        void get_full_block(int32_t index);

        // called via node->exec()
        void get_head_block_index();

        void send_message( const proto::message& msg );
        void handle_write( std::vector<char>* data, const boost::system::error_code& err );

        proto::message               msg;
        uint32_t                     msg_size;
        boost::asio::ip::tcp::socket sock;
        boost::shared_ptr<gpm::node> m_node;
};

/**
 *  
 */
class server 
{
    public:
        server( const boost::shared_ptr<gpm::node>& node )
        :m_node(node), m_acceptor(m_ios)
        {
            the_work = new boost::asio::io_service::work(m_ios);
            m_thread.reset( new boost::thread( boost::bind( &boost::asio::io_service::run, &m_ios ) ) );

            node->new_transaction.connect( boost::bind( &server::send_transaction, this, _1 ) );
            node->new_block.connect( boost::bind( &server::send_full_block, this, _1 ) );
        }
        ~server()
        {
            delete the_work;
            m_ios.stop();
            m_thread->join();
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
                m_connections.push_back( con );
                con->read_message();
                start_accept();
            }
            else
            {
                elog( "Error accepting connection" );
            }
        }

        void connect_to( const std::string& host, uint16_t port )
        {
            std::stringstream ss;
            ss << port;
            using namespace boost::asio::ip;
            tcp::resolver resolver(m_ios);
            tcp::resolver::query query(host, ss.str() );
            tcp::resolver::iterator itr = resolver.resolve(query);
            tcp::resolver::iterator end = resolver.resolve(query);
            
            boost::shared_ptr<connection> con( new connection(m_ios, m_node) );
            boost::system::error_code err = boost::asio::error::host_not_found;
            while( err && itr != end )
            {
                con->sock.close();
                con->sock.connect( *itr++, err );
            }
            if( err )
            {
                throw boost::system::system_error(err);
            }

            // TODO: move this to strand
            m_connections.push_back(con); 

            con->get_full_block( m_node->get_head_block_index() + 1 );
        }

        void send_transaction( const signed_transaction& trx );
        void send_full_block( const full_block_state& blk );

    private:
        boost::shared_ptr<gpm::node>     m_node;
        boost::asio::io_service          m_ios;
        boost::asio::ip::tcp::acceptor   m_acceptor;
        boost::scoped_ptr<boost::thread> m_thread;

        boost::asio::io_service::work*            the_work;
        std::list<boost::shared_ptr<connection> > m_connections;
};

}

#endif
