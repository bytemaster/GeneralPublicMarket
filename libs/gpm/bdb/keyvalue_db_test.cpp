#include <gpm/bdb/keyvalue_db.hpp>
#include <boost/rpc/log/log.hpp>

using namespace gpm::bdb;

int main( int argc, char** argv )
{
    keyvalue_db<std::string,std::string> db;
    db.open( "kv_db_test.db" );

    db.set( "Hello", "world" );
    db.set( "Apple", "one" );
    db.set( "Dan", "two" );
    db.set( "Boo", "three" );

    keyvalue_db<std::string,std::string>::iterator itr = db.find( "Hello" );
    if( !itr.end() )
        slog("Found %1%", itr.value() );
    else
        elog( "Did not find %1%", itr.key() );

    if( itr.value() != std::string( "world" ) )
        elog( "Failed to find proper value." );

    itr = db.begin();
    while( !itr.end() )
    {
        slog("%1% => %2%", itr.key(), itr.value() );
        ++itr;
    }

    db.remove("Hello");
    itr = db.find( "Hello" );
    if( itr.end() )
        slog("Removed %1%", itr.key() );
    else
        elog( "Did not remove %1%", itr.key() );

    return 0;
}
