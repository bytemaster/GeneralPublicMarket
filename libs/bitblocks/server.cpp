#include "block.hpp"
#include "protocol.hpp"
#include "bitcoin.hpp"

boost::asio::io_service::work*      the_work = 0;
user_db&                 get_users_db()     { static user_db users; return users; }
block_db&                get_blocks_db()    { static block_db blocks; return blocks; }
boost::asio::io_service& main_ios()         { static boost::asio::io_service ioserv; return ioserv; }

using namespace boost::asio::ip;

class server
{
    public:
        server( uint16_t port, boost::asio::io_service& i = main_ios() )
        :sock(i, udp::endpoint( udp::v4(), port) )
        {
            start_receive();
        }

        void start_receive()
        {
            sock.async_receive_from(  boost::asio::buffer( recv_buffer ), remote_ep,
                                      boost::bind( &server::handle_receive, this, boost::asio::placeholders::error,
                                                                                  boost::asio::placeholders::bytes_transferred ) );
        }
        void handle_receive( const boost::system::error_code& error, std::size_t transfered )
        {
            if( !error || error == boost::asio::error::message_size )
            {
                message msg;
                boost::rpc::raw::unpack( recv_buffer, msg );
                switch( msg.id )
                {
                    case create_account::id:       handle_create_account( msg );      break;
                    case account_created::id:      handle_close_account( msg );       break;
                    case query_server_status::id:  handle_query_server_status( msg ); break;
                    case request_bitblock::id:     handle_request_bitblock( msg ); break;
                }
            }
            else
            {
                elog( "Error receiving packet" );
            }
            start_receive();
        }

        void handle_create_account( const create_account& msg )
        {
            // check to see if current account exists for user
            // if recv address is still the same, return same payto address
            // else transfer balance to old recv from address, create new payto address, reset balances
        }
        void handle_close_account( const close_account& msg )
        {
            // transfer balance to recv from
        }

        void handle_query_server_status( const query_server_status& msg )
        {

        }

        void handle_request_bitblock( const request_bitblock& rbb )
        {
            // have 2 queues.. above avg and below average

            // the request processing thread first checks above avg
            // then checks below average.  Every message processed
            // adjusts the average.  
            //
            // Someone sends a bunch of high offer requests it increases
            // the average moving more uses into the below average category
            // the circular buffer replaces the oldest request with the
            // newest request
            //
            // The average is a weight over the past 10 seconds.

            // if rbb.offer > min
            // insert into proper queue
            
            /*
            block_db::iterator itr = get_blocks().find( rbb.block_hash );
            if( itr != get_blocks().end() )
            {
                block& b = itr.value();
                uint32_t pos = rbb.start_kb * 1024;
                if( b.data.size() > pos )
                {
                    

                }
            }
            */
        }



        boost::asio::ip::udp::socket  sock;
        udp::endpoint                 remote_ep; 
        std::vector<char>             recv_buffer;
};


int main( int argc, char** argv )
{
    std::string port;
    std::string data_dir;

    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "print this help message." )
        ("port,p", po::value<std::string>(&port)->default_value(8000), "Port to run server on." )
        ("data_dir,d", po::value<std::string>(&data_dir)->default_value("bitblocks"), "Directory to store data" )
    ;

    po::variables_map vm;
    po::store( po::parse_command_line(argc,argv,desc), vm );
    po::notify(vm);

    if( vm.count("help") )
    {
        std::cout << desc << std::endl;
        return 0;
    }
    

    get_users_db().open( data_dir + "/users.db" ); 
    get_blocks_db().open( data_dir + "/blocks.db" ); 

    the_work = new boost::asio::io_service::work(ios);
    ios.run();
}
