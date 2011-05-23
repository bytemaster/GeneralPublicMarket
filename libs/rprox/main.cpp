#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "rproxy.hpp"

boost::asio::io_service::work*  the_work = 0;

void setup_port_map( rproxy::server* s, const std::string& map_port );

int main( int argc, char** argv )
{
    std::string data_dir;
    std::string config = "rproxy.cfg";
    std::string host_port;
    std::string map_port;
    uint16_t    port = 0;

    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "print this help message." )
        ("listen,l", po::value<uint16_t>(&port), "Port to run server on." )
        ("config,c", po::value<std::string>(&config), "Configuration file to use" )
        ("rproxy,r", po::value<std::string>(&host_port), "HOST:PORT of the reverse proxy" )
        ("server,s", po::value<std::string>(&map_port), "HOST:PORT=REMOTE_PORT server to expose" )
        ("data_dir,d", po::value<std::string>(&data_dir)->default_value("rproxy_data"), "Directory to store data" )
    ;

    po::variables_map vm;
    po::store( po::parse_command_line(argc,argv,desc), vm );
    po::notify(vm);

    if( vm.count("help") )
    {
        std::cout<< "Examples:\n";
        std::cout<< "\tStart a reverse proxy server on port 8000\n";
        std::cout<< "\t\t./rproxy -l 8000\n";
        std::cout<< "\n\tMap localhost 8001 to localhost 9000 via the rproxy running at localhost:8000\n";
        std::cout<< "\t\t./rproxy -r localhost:8000 -s localhost:9000=8001 \n\n";
        std::cout << desc << std::endl;
        return 0;
    }

    boost::asio::io_service ios;

    rproxy::server s( ios, data_dir );

    if( port )
        s.listen( port );
    if( host_port.size() )
        s.connect_to( host_port, boost::bind( setup_port_map, &s, map_port ) );

    the_work = new boost::asio::io_service::work(ios);
    ios.run();
    return 0;
}


void setup_port_map( rproxy::server* s, const std::string& map_port )
{
    if( map_port.size() )
    {
        std::string host_port = map_port.substr( 0, map_port.find('=') );
        std::string remote_port = map_port.substr(host_port.size()+1);
        std::stringstream rp(remote_port);
        uint16_t p; rp >> p;
        s->map_port( host_port, p );
    }
}
