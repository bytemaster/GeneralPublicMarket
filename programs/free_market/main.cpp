#include <gpm/node/node.hpp>
#include <boost/program_options.hpp>
#include <QtConcurrentRun>
#include <gpm/time/usclock.hpp>

//#include <gpm/client.hpp>
#include <gpm/bdb/keyvalue_db.hpp>
#include <gpm/node/server.hpp>

#include <QSettings>
#include <QDir>
#include <QApplication>
#include "keychain.hpp"
//#include <gui/MainWindow.hpp>


using namespace gpm;


const boost::shared_ptr<node>& get_node()
{
    static boost::shared_ptr<node> n(new node());
    return n;
}

bool sign( signed_transaction& trx, const node::ptr& n   );
void transfer( signed_transaction& trx, uint64_t amnt, const std::string& shares, 
               const std::string& from, const std::string& to, const node::ptr& n  );


std::string stdin_getline()
{
    std::string rtn;
    std::getline(std::cin,rtn);
    return rtn;
};

int main( int argc, char** argv )
{
    QApplication app(argc,argv);
    QCoreApplication::setOrganizationName("Virtual Ventures");
    QCoreApplication::setOrganizationDomain("virtualventures.com");
    QCoreApplication::setApplicationName("Free Market");

 //   MainWindow* mw = new MainWindow();
 //   mw->show();

    try {
        std::string gen_name("none" );
        std::string data_dir( "gpm_data" );
        bool do_create;
        uint16_t server_port = 8000;
        std::string backbone_network("239.255.255.1:4794");
        std::string urn("free_market");
        std::string keys("gpm_keys.db" );
        std::vector<std::string> clients;

        namespace po = boost::program_options;
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help,h", "print this help message." )
            ("gen_name,n", po::value<std::string>(&gen_name), "Name of the generator, 'none' if node is not generating" )
            ("data_dir,d", po::value<std::string>(&data_dir), "Directory to store data" )
            ("create,c", po::value<bool>(&do_create), "Create new chain" )
            ("keychain,k", po::value<std::string>(&keys)->default_value(keys), "File containing private keys" )
            ("client,C", po::value<std::vector<std::string> >(&clients), "One or more client to connect to HOST:PORT" )
            ("server_port,p",    po::value<uint16_t>(&server_port)->default_value(server_port), "The URN to be used by this node" )
        ;

        po::variables_map vm;
        po::store( po::parse_command_line(argc,argv,desc), vm );
        po::notify(vm);

        if( vm.count("help") )
        {
            std::cout << desc << std::endl;
            return 0;
        }
    
//        bb::network_ptr net( new bb::network() );
//        wlog( "Backbone network: %1%", %backbone_network );
//        net->join_network( fl::udp_endpoint_from_string( backbone_network ) );

//        bb::server_ptr rd_server( new bb::server( get_node(), urn) );
//        net->add_server(rd_server);


        //keychain::ptr keyc( new keychain() ); // NULL, "keychain.db", "keychain" );
        //name_key_map::ptr nkm( new name_key_map() ); // NULL, "keychain.db", "keychain" );
        get_keychain().open( keys );
        //nkm->open( "name_key_map.db" );
        get_node()->open( data_dir, true );

//        gpm::client* c = new gpm::client( get_node(), net );

        gpm::server server(get_node(), server_port);
        for( uint32_t i = 0; i < clients.size(); ++i )
            server.connect_to( clients[i] );

        boost::shared_ptr<node> n = get_node();



        bool done = false;
        gpm::signed_transaction trx;
        while( !done )
        {
            std::cout<<"]$ ";
            QFuture<std::string> s = QtConcurrent::run( stdin_getline );
            while( !s.isFinished() )
            {
                app.processEvents();
                usleep(1000 * 10 );
            }
            std::string line = s.result();
            std::stringstream ss(line);
            std::string cmd;
            ss >> cmd;

            if( cmd == "exit" )
            {
                wlog( "exit!" );
                app.quit();
                done = true;
                continue; 
            }
            else if( cmd == "help" )
            {
                std::cout << "generate name                   - start generating with the given name\n";
                std::cout << "start                           - start a new transaction\n";
                std::cout << "commit                          - submit the completed transaction to the network\n";
                std::cout << "transfer amount stock src dst   - transfers stock of type from 'from' to 'to'\n";
                std::cout << "issue stock                     - issues a new stock\n";
                std::cout << "abort                           - reset the the transaction\n";
                std::cout << "exit                            - quit gpm\n";
                std::cout << "ln                              - list all names\n";
                std::cout << "contents account                - list all balances in account\n";
                std::cout << "balance  account stock          - print the balance of the account\n";
                std::cout << "register  name                  - generates a pub/priv key and r \n";
                std::cout << "address_of name                 - generates a pub/priv key and r \n";
                std::cout << "genkey [alias]                  - generates a key";
                std::cout << "list_keys                       - lists keys";
                std::cout << "dump start len                  - dump trx";
            }
            else if( cmd == "ln" )
            {
                std::cerr<<"ln: not implemented\n";
            }
            else if( cmd == "contents" )
            {
                std::string name;
                ss >> name;
                std::vector<std::string> c(get_node()->get_account_contents( name ));
                for( uint32_t i = 0; i < c.size(); ++i )
                {
                    std::cerr << c[i] << " => " << get_node()->get_balance( name, c[i] ) << std::endl;
                }
            }
            else if( cmd == "genkey" )
            {
                std::string name;
                ss >> name;

                public_key_t pu; 
                private_key_t pr;
                std::string address = generate_address(pu,pr);
                get_keychain().set( pu, keychain_record(pu,pr,name) );
            }
            else if( cmd == "address_of" )
            {
                std::string name;
                ss >> name;
                std::cerr << "address_of( " << name << ") == " << encode_address( get_node()->get_public_key_t( name ) ) << std::endl; 
            }
            else if( cmd == "list_keys" )
            {
                keychain::iterator itr = keychain::get_default().begin();
                while( !itr.end() )
                {
                    std::cerr << itr.key().address << "  '" << itr.value().alias << "'\n";
                    ++itr;
                }
            }
            else if( cmd == "generate" ) 
            {
                std::string name;
                ss >> name;
                get_node()->configure_generation( name, name != "off" );
            }
            else if( cmd == "dump" )
            {
                int start = 0;
                int len   = 0;
                ss >> start >> len;
                n->dump( start, len );
            }
            else if( cmd == "register" )
            {
                std::string name;
                ss >> name;

                gpm::public_key_t old_public_key_t;
                gpm::private_key_t old_private_key_t;
                gpm::key_info ki;

                bool is_valid = false;
                bool sign = false;
                if( n->get_key_for_name( name, old_public_key_t ) )
                {
                    if( get_keychain().get( gpm::encode_address(old_public_key_t), ki ) )
                    {
                        wlog( "Name already registered to key, signing with old key" );
                        is_valid = true;
                        sign = true;
                    }
                    else
                    {
                        elog( "Name already registered and no private key on keychain" );
                    }
                }
                else
                {
                    is_valid = true;
                }
                if( is_valid )
                {
                    gpm::cmd::register_name reg;
                    reg.name = name;

                    gpm::private_key_t priv;
                    gpm::key_info ki;
                    std::string addr = gpm::generate_address(ki.pub,ki.priv);
                    ki.alias = name;
                    reg.pub_key = ki.pub;
                    
                    get_keychain().set( addr, ki );

                    get_keychain().sync();
                    trx.trx.commands.resize( trx.trx.commands.size() + 1 );
                    trx.trx.commands.back().id = gpm::cmd::register_name::id;
                    boost::rpc::raw::pack( trx.trx.commands.back().data, reg );

                    QSettings pref;
                    pref.beginGroup( "Accounts" );
                    QStringList acnts = pref.childGroups();
                    pref.setValue( (name + "/index").c_str(), acnts.size() );
              //      nkm->set( name,reg.pub_key );
                }
                if( sign )
                {
                    trx.sign( ki.priv );
                }
            }
            else if( cmd == "issue" )
            {
                std::string stock_name;
                ss >> stock_name;
                if( n->has_balance( stock_name,stock_name ) )
                {
                    elog( "%1% has already been issued.", stock_name );
                }
                else
                    trx.trx.commands.push_back( cmd::issue(stock_name ) );
            }
            else if( cmd == "balance" )
            {
                std::string stock_name;
                std::string account;
                ss >> account >> stock_name;
                slog( "%1% / %2% -> %3%", account, stock_name, n->get_balance(account,stock_name) );
            }
            else if( cmd == "transfer" )
            {
                int64_t amt = 0;
                std::string stock_name;
                std::string from;
                std::string to;
                ss >> amt >> stock_name >> from >> to;
                transfer( trx, amt, stock_name, from, to, n );
            }
            else if( cmd == "start" )
            {
                trx = gpm::signed_transaction();
            }
            else if( cmd == "commit" )
            {
                trx.trx.utc_time = gpm::utc_clock();
                wlog( "us_time %1%", trx.trx.utc_time );
                if( sign(trx, n) )
                    n->add_transaction( trx );
                else
                    elog( "Error signing transaction." );
                trx = gpm::signed_transaction();
            }
            else if( cmd == "abort" )
            {
                trx = gpm::signed_transaction();
            }
            else
            {
                elog( "Unknown command '%1%'", line );
            }
        }
        
        //app.exec();
    } 
    catch ( const boost::exception& e )
    {
        elog( "exception: %1%", boost::diagnostic_information(e) );
    }
}
/**
 * Applies any signatures required by the commands
 */
bool sign( signed_transaction& trx, const node::ptr& n )
{
    try {
    std::vector<std::string> sigs;
    for( uint32_t c = 0; c < trx.trx.commands.size(); ++c )
    {
        if( trx.trx.commands[c].id == cmd::register_name::id )
        {
             cmd::register_name rn = trx.trx.commands[c];
             gpm::public_key_t old_public_key_t;
             gpm::private_key_t old_private_key_t;
             gpm::key_info ki;
            
             if( std::find(sigs.begin(),sigs.end(),rn.name) != sigs.end() )
                continue;
             if( rn.name == gpm::encode_address( rn.pub_key ) )
             {
                 gpm::key_info ki;
                 if( get_keychain().get( rn.name, ki ) )
                 {
                     trx.sign(ki.priv);
                     sigs.push_back( rn.name );
                 }
                 else
                 {
                    elog( "Do not have private key for account %1%", rn.name );
                 }
             }
             else if( n->get_key_for_name( rn.name, old_public_key_t ) )
             {
                 if( get_keychain().get( old_public_key_t, ki ) )
                 {
                     wlog( "Name already registered to key, signing with old key" );
                     trx.sign(ki.priv);
                 }
                 else
                 {
                    elog( "Error signing transaction." );
                    return false;
                 }
             }
        }
        else if( trx.trx.commands[c].id == cmd::transfer::id )
        {
             cmd::transfer tr = trx.trx.commands[c];

             if( std::find(sigs.begin(),sigs.end(),tr.from_name) != sigs.end() )
                continue;

             gpm::public_key_t old_public_key_t;
             gpm::private_key_t old_private_key_t;
             gpm::key_info ki;

             if( tr.from_name.size() )
             {
                 if( n->get_key_for_name( tr.from_name, old_public_key_t ) )
                 {
                     if( get_keychain().get( gpm::encode_address(old_public_key_t), ki ) )
                     {
                         trx.sign(ki.priv);
                         sigs.push_back(tr.from_name);
                     }
                     else
                        return false;
                 }
                 else
                    return false;
             }
        }
        else if( trx.trx.commands[c].id == cmd::issue::id )
        {
             cmd::issue is = trx.trx.commands[c];
             slog( "signing issue %1%", is.stock_name );

             if( std::find(sigs.begin(),sigs.end(),is.stock_name) != sigs.end() )
                continue;

             gpm::public_key_t old_public_key_t;
             gpm::private_key_t old_private_key_t;
             gpm::key_info ki;
             if( n->get_key_for_name( is.stock_name, old_public_key_t ) )
             {
                 if( get_keychain().get( old_public_key_t, ki ) )
                 {
                     sigs.push_back(is.stock_name);
                     trx.sign(ki.priv);
                 }
                 else
                 {
                    elog( "Unable to find private key for public key" );
                    return false;
                 }
             }
             else
             {
                elog( "unable to find public key for name %1%", is.stock_name );
                return false;
             }
        }
        else
            return false;

    }
    return true;
    } catch ( ... )
    {
        elog( "Error signing transaction" );
        return false;
    }
}
void transfer( signed_transaction& trx, uint64_t amnt, const std::string& shares, 
               const std::string& from, const std::string& to, const node::ptr& n  )
{
     slog( "transfer %1%  %2%  from '%3%' to '%4%'", amnt, shares, from, to );
     gpm::public_key_t from_pub_key;
     gpm::public_key_t to_pub_key;
     gpm::public_key_t share_pub_key;
     gpm::private_key_t from_priv_key;
     gpm::private_key_t share_priv_key;

     if( to != "gen" && !validate_address_format(to) )
     {
         if( !n->get_key_for_name( to, to_pub_key ) )
         {
              elog( "%1% is not a known name", to );
              return;
         }
     }
     if( !n->get_key_for_name( shares, share_pub_key ) )
     {
          elog( "%1% is not a known name", shares );
          return;
     }
     if( !n->get_key_for_name( from, from_pub_key ) )
     {
          elog( "%1% is not a known name", from );
          return;
     }
     uint64_t from_bal = n->get_balance(  from, shares );
     if( from_bal < amnt )
     {
          elog( "Insufficient %1% in account %2%, available balance %3%", shares, from, from_bal );
          return;
     }
     gpm::key_info ki;
     if( !get_keychain().get( gpm::encode_address(from_pub_key),ki ) )
     {
        if( validate_address_format( from ) )
        {
            if( !get_keychain().get( from,ki ) )
            {
                elog( "We do not know the public key for account %1%", to );
                return;
            }
            else
            {
                wlog( "Chain does not know public key, so lets register our public key before transfering from %1%", from );
                gpm::cmd::register_name reg;
                reg.name = from;
                reg.pub_key = ki.pub;
                trx.trx.commands.push_back( reg );
            }
        }
        else
        {
          elog( "No private key for source account: %1%", from ); 
        }
     }
     // SUCCESS  we have the private key for 'from' with enough 'shares' to transfer to 'to'.
    cmd::transfer tran;
    tran.stock_name = shares;
    tran.from_name = from;
    if( to != "gen" )
        tran.to_name = to;
    tran.amount = amnt;

    trx.trx.commands.push_back( tran );
    return;
}
