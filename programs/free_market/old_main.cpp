#include <fl/scheduler/main.hpp>
#include <QApplication>
#include "MainWindow.hpp"
#include <dtdb/keychain.hpp>
#include <dtdb/server.hpp>
#include <dtdb/node.hpp>
#include <QSettings>
#include <QDir>

using namespace dtdb;

void fl_main( int argc, char** argv )
{
    debug_config( status.enable(true) );

    uint16_t    server_port = 8000;
    std::vector<std::string> remote_hosts;
    std::string gen_name("none" );
    std::string data_dir( "dtdb_data" );
    bool c;


    QApplication app(argc,argv);
    QCoreApplication::setOrganizationName("Virtual Ventures");
    QCoreApplication::setOrganizationDomain("virtualventures.com");
    QCoreApplication::setApplicationName("Free Market");

    QSettings    pref;
    server_port = pref.value( "server/port", server_port ).toInt();
    gen_name    = pref.value( "generator/name", gen_name.c_str() ).toString().toStdString();
    data_dir    = pref.value( "node/data_directory", QDir::homePath() + "/free_market_data" ).toString().toStdString();


    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "print this help message." )
        ("server,s",   po::value<std::vector<std::string> >(&remote_hosts), "One or more IP:PORT to connect to." )
        ("port,p",     po::value<uint16_t>(&server_port)->default_value(server_port), "The TCP Server Port" )
        ("gen_name,n", po::value<std::string>(&gen_name)->default_value(gen_name), "Name of the generator, 'none' if node is not generating" )
        ("data_dir,d", po::value<std::string>(&data_dir)->default_value(data_dir), "Directory to store data" )
        ("create,c", po::value<bool>(&c), "Create new chain" )
    ;
    desc.add(fl::dbg::program_options());

    po::variables_map vm;
    po::store( po::parse_command_line(argc,argv,desc), vm );
    po::notify(vm);

    if( vm.count("help") )
    {
        std::cout << desc << std::endl;
        fl::scheduler::current()->quit(1);
        return;
    }

    rout( status, "server port %1%", %server_port );
    dtdb::server::ptr s(new dtdb::server(server_port) );

    // create the node, and give it the server
    dtdb::node::ptr   n( new dtdb::node(s, gen_name, data_dir, c) );

    // add connections
    for( uint32_t i = 0; i < remote_hosts.size(); ++i )
    {
        dtdb::client::ptr c;
        c = dtdb::client::ptr( new dtdb::client( ) );
        if( c->connect_to( fl::endpoint_from_string( remote_hosts[i] ) ) )
            s->add_client(c);
        else
            rout( warn, "Unable to connect to %1%", %remote_hosts[i] );
    }
    
    // check to see if there is a newer block chain
    n->find_current_blockchain();

    keychain::ptr keyc( new keychain( NULL, "keychain.db", "keychain" ) );

    MainWindow* mw = new MainWindow(keyc, n);
    mw->show();
    
    while( fl::scheduler::current()->is_running() )
    {
        // process Qt events for up to 10 ms
        QCoreApplication::processEvents( QEventLoop::AllEvents, 10 /*ms*/ );
        // let other tasks run for 10 ms
        fl::scheduler::usleep(10*1000);
    }

    delete mw;
}
