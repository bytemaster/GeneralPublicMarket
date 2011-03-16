#include <gpm/statedb/state_database.hpp>
#include <gpm/crypto/crypto.hpp>

using namespace gpm;

int main( int argc, char** argv )
{
    try {
    slog("creating test public keys" );
    public_key_t  pub_key[4]; 
    private_key_t priv_key[4]; 
    for( uint32_t i = 0; i < 4; ++i )
        generate_keys( pub_key[i], priv_key[i] );

    {
        boost::shared_ptr<state_database> m_sdb(new state_database());

        m_sdb->open( "test_state_db.dat" );
        if( !m_sdb->set_public_key( "dan", pub_key[0] ) )
        {
            elog( "Error setting key for dan." );
        }
        m_sdb->set_public_key( "scott", pub_key[1] );

        public_key_t pk2;
        if( !m_sdb->get_public_key_t( "dan", pk2 ) )
        {
            elog( "Error unable to find public key for 'dan' " );
            return -1;
        }
        if( pk2 != pub_key[0] )
        {
            elog( "Public keys do not match" );
            return -1;
        }

        m_sdb->set_public_key( "dan", pub_key[3] );
        m_sdb->set_public_key( "dollar", pub_key[2] );
        m_sdb->set_public_key( "euro", pub_key[1] );
        if( !m_sdb->get_public_key_t( "dan", pk2 ) )
        {
            elog( "Error unable to find public key for 'dan' " );
            return -1;
        }
        if( !(pk2 == pub_key[3]) )
        {
            elog( "Public keys do not match after updating." );
            return -1;
        }

        slog("issue shares..." );
        try {
        m_sdb->issue( "dollar" );
        m_sdb->issue( "euro" );
        m_sdb->issue( "dan" );
        }catch( const gpm::exception& e )
        {
            wlog( "caught exception: %1%", boost::diagnostic_information(e) );
        }
        m_sdb->transfer_balance( "dan", "scott", "dan", 100 );
        m_sdb->transfer_balance( "dan", "scott", "dan", 500 );
        m_sdb->transfer_balance( "scott", "dan", "dan", 9 );
        m_sdb->transfer_balance( "scott", "dan", "dan", 8 );
        m_sdb->transfer_balance( "dollar", "dan", "dollar", 10000 );
        m_sdb->transfer_balance( "dan", "scott", "dollar", 500 );


        bool is_null = true;
        uint64_t sbal = m_sdb->get_balance( "scott", "dan", &is_null );

        slog("scott:dan = %1%", sbal );
        
        m_sdb->commit();

        m_sdb->transfer_balance( "dan", "scott", "dan", 1000 );
        m_sdb->transfer_balance( "dan", "scott", "dan", 500 );
        m_sdb->transfer_balance( "scott", "dan", "dan", 10 );
        m_sdb->transfer_balance( "scott", "dan", "dan", 20 );


        sbal = m_sdb->get_balance( "scott", "dan", &is_null );
        slog("scott:dan = %1%", sbal );


        wlog( "starting trx1" );
        state_database_transaction::ptr trx( new state_database_transaction( m_sdb ) );
        trx->transfer_balance( "scott", "dan", "dollar", 123 );
        trx->commit();
        wlog( "commit trx1" );
        wlog( "starting trx2" );
        state_database_transaction::ptr trx2( new state_database_transaction( trx ) );
        trx2->transfer_balance( "scott", "dan", "dollar", 321 );
        wlog( "commit trx2" );
        trx2->commit();
        wlog( "commit trx1" );
        trx->commit();

        abstract_state_database::transfer tran = m_sdb->get_last_transfer( "scott", "dollar" );
        while( !tran.end() )
        {
            slog("from: %1%  to: %2%  type:%3% amount: %4%  to_balance: %5% from_balance: %6%",
                           tran.from(), tran.to(),  tran.type(),  tran.amount(), tran.to_balance(), tran.from_balance());
            tran.previous("scott");
        }
        tran = m_sdb->get_last_transfer( "dan", "dollar" );
        while( !tran.end() )
        {
            slog("from: %1%  to: %2%  type:%3% amount: %4%  to_balance: %5% from_balance: %6%",
                           tran.from(), tran.to(),  tran.type(),  tran.amount(), tran.to_balance(), tran.from_balance());
            tran.previous("dan");
        }
    }

    state_database m_sdb2;
    m_sdb2.open( "test_state_db.dat" );
        bool is_null = true;
    uint64_t sbal = m_sdb2.get_balance( "scott", "dan", &is_null );

    slog("scott:dan = %1%", sbal );


    } catch ( const boost::exception& e )
    {
        elog( "caught exception: %1%", boost::diagnostic_information(e) );
    }

    return 0;
}
