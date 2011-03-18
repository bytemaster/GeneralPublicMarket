#include <gpm/node/node.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <gpm/statedb/state_database.hpp>
#include <gpm/bdb/keyvalue_db.hpp>
#include <gpm/time/usclock.hpp>
#include <boost/rpc/super_fast_hash.hpp>
#include <boost/rpc/json.hpp>

inline bool operator < ( const std::pair<bool, boost::rpc::sha1_hashcode>& l, 
                         const std::pair<bool, boost::rpc::sha1_hashcode>& r )
{
    return l.first < r.first && l.second < r.second;
}
inline bool operator > ( const std::pair<bool, boost::rpc::sha1_hashcode>& l, 
                         const std::pair<bool, boost::rpc::sha1_hashcode>& r )
{
    return l.first > r.first && l.second > r.second;
}
inline bool operator == ( const std::pair<bool, boost::rpc::sha1_hashcode>& l, 
                          const std::pair<bool, boost::rpc::sha1_hashcode>& r )
{
    return l.first == r.first && l.second == r.second;
}

namespace gpm {

enum transaction_state
{
    PENDING_TRX = 0,
    HEAD_TRX    = 1,
    APPLIED_TRX = 2
};

class node_private
{
    public:
        node* self;
        node_private(node* s)
        :self(s)
        {
            m_gen_enabled    = false;
            m_trx_db         = NULL;
            m_block_state_db = NULL;
            slog( "Calculating hash rate...." );
            m_hashrate = calculate_hash_per_sec( 1000 * 100 * 1  );
            slog( "%1% hash / sec", m_hashrate );


            m_generating = false;
            m_stop_gen = false;
            m_gen_stopped = true;
        }
        ~node_private()
        {
            m_stop_gen = true;
            while( !m_gen_stopped ){}
        }

        block_chain         m_block_chain;
        state_database::ptr m_state_db;

        uint64_t        m_hashrate;

        boost::filesystem::path m_datadir;
        std::string     m_gen_name;
        bool            m_gen_enabled;
        /**
         *  This database stores all known transactions and whether or not
         *  they have been applied to the database.
         *
         *  Transactions are sorted by state;
         *      0) Not yet applied in any generated block
         *      1) Applied in the head
         *      2) Applied and confirmed (not head)
         *
         */
        bdb::keyvalue_db<std::pair<int,boost::rpc::sha1_hashcode>,signed_transaction >*  m_trx_db;
        bdb::keyvalue_db<boost::rpc::sha1_hashcode, block_state>*                        m_block_state_db;

        state_database_transaction::ptr m_head_trx;
        state_database_transaction::ptr m_gen_trx;


        void dump( const block_chain& bc, uint32_t s = 0, uint32_t l = 10000 );
        void dump( const signed_transaction& );
        /**
         *   This method may yield while waiting 
         */
        boost::optional<block_state> find_block_state( const boost::rpc::sha1_hashcode& hc )
        {
            // TODO: if not local, the query remote hosts.
            return m_block_state_db->get(hc);
        }
        std::vector<boost::rpc::sha1_hashcode> get_head_transactions()
        {
            // get state from head and return it...
            boost::optional<block_state> bs = m_block_state_db->get(m_block_chain.back().state);
            if( !bs )
                THROW_GPM_EXCEPTION( "Unable to find block_state for head." );
            return bs->signed_transactions;
        }
        std::vector<signed_transaction> get_transactions( const std::vector<boost::rpc::sha1_hashcode>& trx, int group )
        {
            std::vector<signed_transaction> strx(trx.size());
            for( uint32_t i = 0; i < trx.size(); ++i )
            {
                slog( "looking for %1% in group %2%", trx[i], group );
                boost::optional<signed_transaction> s = m_trx_db->get( std::make_pair(group,trx[i]) );
                if( !s )
                {
                    elog( "not found!" );
                    strx.resize(0);
                    return strx;
                }
                strx[i] = *s;
            }
            return strx;
        }
        void move_transactions(  int from_group, int to_group )
        {
            bdb::keyvalue_db<std::pair<int,boost::rpc::sha1_hashcode>,signed_transaction >::iterator itr = 
                                                    m_trx_db->search( std::make_pair( from_group, boost::rpc::sha1_hashcode() ) );
            while( !itr.end() && itr.key().first == from_group)
            {
                slog( "%1% to %2%", boost::rpc::to_json(itr.key() ), to_group );
                m_trx_db->set( std::make_pair( to_group, itr.key().second ), itr.value() );
                std::pair<int,boost::rpc::sha1_hashcode> rm = std::make_pair( from_group, itr.key().second );
                if( !m_trx_db->remove( rm ) )
                {
                    elog( "Error removing %1%", boost::rpc::to_json(rm) );
                }
                itr = m_trx_db->search( std::make_pair( from_group, boost::rpc::sha1_hashcode() ) );
            }
        }
        void move_transactions(  const std::vector<boost::rpc::sha1_hashcode>& trx, int from_group, int to_group )
        {
            for( uint32_t i = 0; i < trx.size(); ++i )
            {
                bdb::keyvalue_db<std::pair<int,boost::rpc::sha1_hashcode>,signed_transaction >::iterator itr = 
                                                        m_trx_db->find( std::make_pair( from_group, trx[i] ) );
                if( itr.end() )
                {
                    //THROW_GPM_EXCEPTION( "Unable to find transaction %1% in group %2%", %trx[i] %from_group );
                    wlog(  "Unable to find transaction %1% in group %2%", trx[i], from_group );
                }
                else
                {
                    m_trx_db->set( std::make_pair( to_group, trx[i]), itr.value() );
                    m_trx_db->remove( std::make_pair(from_group,trx[i]) );
                  //  itr.remove();
                }
            }
        }

        void synchronize_state()
        {
            slog( "synchronizing the known state." );
            block b = m_state_db->find_last_block();
            for( int32_t i = m_block_chain.size() - 1; i >= 0; --i )
            {
                if( m_block_chain[i].prev_block == b.prev_block )
                {
                    for( uint32_t x = i+1; x < m_block_chain.size(); ++x )
                    {
                        wlog(  "Need to reverse state for block %1%", boost::rpc::to_json(m_block_chain[x]) );
                    }
                    m_block_chain.resize(i+1);
                    return;
                }
            }
            m_state_db->update_index();
        }


        // generation state vars
        boost::rpc::sha1_hashcode            m_hash_target;
        bool                         m_generating;
        volatile bool                m_stop_gen;
        volatile bool                m_gen_stopped;
        void start_block()
        {
            slog( "starting block" );
            m_generating = true;
            m_stop_gen = true;

            m_gen_trx->abort();
            if( m_block_chain.size() == 0 )
            {
                block_state bs;

                bs.index          = 0;
                bs.generator_name = m_gen_name;
                bs.state_db       = m_state_db->calculate_state_hash();

                block       b;
                b.utc_time        = gpm::utc_clock() / 1000000;
                b.state           = boost::rpc::raw::hash_sha1(bs);

                if( gpm::generate( b, get_max_hash() ) )
                {
                    if( !m_head_trx->apply( b, m_gen_name, std::vector<signed_transaction>(), bs.state_db ) )
                    {
                        elog( "Error applying first block." );
                    }
                }
                m_block_state_db->set( b.state, bs );
                m_block_chain.push_back(b);
            }

            block new_block;
            new_block.prev_block = boost::rpc::raw::hash_sha1( m_block_chain.back() );
            wlog(  "generating prev_block = %1%", new_block.prev_block );

            block_state new_state;
            new_state.index = m_block_chain.size();
            new_state.generator_name = m_gen_name;

            
            slog( "searching for trx" );
            bdb::keyvalue_db<std::pair<int,boost::rpc::sha1_hashcode>,signed_transaction >::iterator 
                    itr = m_trx_db->search( std::make_pair( PENDING_TRX, boost::rpc::sha1_hashcode() ) );
            while( !itr.end() && itr.key().first == PENDING_TRX  )
            {
                wlog(  "found trx, try to appy it" );
                state_database_transaction::ptr  tmp_trx( new state_database_transaction( m_gen_trx ) );
                slog("key: %1%  value: ", boost::rpc::to_json(itr.key())  );
                    dump( itr.value() );
                if( tmp_trx->apply( itr.value(), m_gen_name ) )
                {
                    new_state.signed_transactions.push_back(itr.key().second);
                    tmp_trx->commit();
                }
                else
                {
                    elog( "Error applying transaction." );
                }
                ++itr;
            }

            new_state.state_db       = m_gen_trx->calculate_state_hash();
            slog( "new_state %1%", new_state.state_db );
            new_block.utc_time = gpm::utc_clock() / 1000000 + SECONDS_PER_BLOCK;
            new_block.state = boost::rpc::raw::hash_sha1(new_state);
            m_block_state_db->set( new_block.state, new_state );

            while( !m_gen_stopped ){} // block until it finished the last

            m_hash_target = calculate_hash_target( m_block_chain );

            m_gen_stopped = false;
            m_stop_gen    = false;
            slog( "starting to generate block target difficulty %1%", (160 -to_bigint(m_hash_target).log2() ) );
// TODO            m_gen_thread.get_scheduler()->schedule<void>(boost::bind( &node_private::generate, this, new_block, m_hash_target ), "generate" );
        }

    void generate( const block& b, const boost::rpc::sha1_hashcode& _target )
    {
        m_gen_stopped = false;
        gpm::block best;
        gpm::block working = b;
        boost::rpc::sha1_hashcode target = _target;
        uint64_t start_time = gpm::usclock();
        bool found = false;
        do {
            working.utc_time = std::max( b.utc_time, uint32_t(gpm::utc_clock() / 100000) ); 

            if( gpm::generate( working, target, working.nonce + 1, working.nonce + m_hashrate ) )
            {
                found = true;
                best = working;
                boost::rpc::raw::hash_sha1( working, target );
                //slog( "found hash %1%", %target );
            }
            //uint32_t utc_c = gpm::utc_clock()/1000000;
            //slog( "ct %1%  target time %1%", %utc_c  %b.utc_time );
        } while( !m_stop_gen && (!found || (gpm::utc_clock() / 1000000 ) < b.utc_time) );



        if( found && gpm::utc_clock()/1000000 >= b.utc_time )
        {
            uint64_t end_time = gpm::usclock();
            slog( "Done generating, found block in %1% s", (double(end_time-start_time)/1000000.0) );
//TODO       m_sch->schedule<void>( boost::bind( &node_private::generated_block, this, best ), "generated_block"  );
            m_stop_gen = true;
        }
        else
        {
            //wlog(  "removing block state %1%", %b.state );
            //m_block_states->remove<block_state>( b.state );
            slog( "Done generating, no block found." );
        }
        m_gen_stopped = true;
    }

    void generated_block( const block& b )
    {
        try {
            if( self->add_block(b) > 0 )
            {
                slog( "Successfully generated and added a block." );
               // this->dump(m_block_chain);
            }
            else
            {
                elog( "Unable to add generated block" );
            }
        }
        catch ( const boost::exception& e )
        {
            elog( "exception: %1%", boost::diagnostic_information(e) );
        }
    }
};


node::node()
{
    my = new node_private(this);
}

node::~node()
{
    if( my->m_trx_db )
        delete my->m_trx_db;
    if( my->m_block_state_db )
        delete my->m_block_state_db;
    delete my;
}


void node::open( const boost::filesystem::path& data_dir, bool create )
{
    if( !boost::filesystem::exists( data_dir ) )
    {
        if( create )
            boost::filesystem::create_directory( data_dir );
        else
            THROW_GPM_EXCEPTION( "Directory '%1%' does not exist.", %data_dir );
    }
    if( !boost::filesystem::is_directory( data_dir ) )
    {
        THROW_GPM_EXCEPTION( "Directory '%1%' is not a directory.", %data_dir );
    }
    my->m_datadir = data_dir;

    my->m_trx_db         = new bdb::keyvalue_db< std::pair<int,boost::rpc::sha1_hashcode>, signed_transaction >();
    my->m_trx_db->open(  data_dir / "trx_db" );
    my->m_block_state_db = new bdb::keyvalue_db<boost::rpc::sha1_hashcode, block_state>(); 
    my->m_block_state_db->open( data_dir / "block_state_db" );
    my->m_state_db = state_database::ptr(new state_database());

    if( !my->m_state_db->open( data_dir/"state_db" ) )
    {
        THROW_GPM_EXCEPTION( "Unable to open state database: %1%", %(data_dir/"state_db") );
    }
    if( !load( my->m_datadir / "blockchain", my->m_block_chain ) )
    {
        wlog(  "No current blockchain." );
    }
    else
        my->dump( my->m_block_chain );
    my->m_head_trx = state_database_transaction::ptr( new state_database_transaction( my->m_state_db ) );
    my->m_gen_trx  = state_database_transaction::ptr( new state_database_transaction( my->m_head_trx ) );

    // clean up the known state.
    my->synchronize_state();

    wlog(  "initial head trx %1%    gen_trx: %2%   file: %3%", my->m_head_trx.get(),  my->m_gen_trx.get(), my->m_state_db.get() );
    my->m_state_db->dump();
}


void node::configure_generation( const std::string& gn, bool on )
{
    my->m_gen_name    = gn;
    my->m_gen_enabled = on;
    generation_state_changed(gn,on);
    if( on )
    {
        my->start_block();
    }
}

bool node::is_generating()const
{
    return my->m_gen_enabled;
}

const std::string& node::generation_name()const { return my->m_gen_name; }

uint64_t node::get_hashrate()const 
{ 
    return my->m_hashrate; 
}
signed_transaction node::get_transaction( const boost::rpc::sha1_hashcode& trx_h )
{
    if( !my->m_trx_db )
        THROW_GPM_EXCEPTION( "Database not open." );

    for( uint32_t i = 0; i < 4; ++i )
    {
        boost::optional<signed_transaction > trx = my->m_trx_db->get(std::make_pair(i,trx_h) );
        if( !!trx )
            return *trx;
    }
    THROW_GPM_EXCEPTION( "Unknown Transaction %1%", %trx_h );
}

full_block_state node::get_full_block( uint32_t index )
{
    if( index > get_head_block_index() )
        return full_block_state( -1, -1 );

    full_block_state fbs;
    fbs.index             = index;
    fbs.head_index        = get_head_block_index(); 
    fbs.blk               = get_block_by_index(index);
    fbs.blk_state         = get_block_state( fbs.blk.state );
    fbs.trxs.resize( fbs.blk_state.signed_transactions.size() );
    for( uint32_t i = 0; i < fbs.trxs.size(); ++i )
    {
        fbs.trxs[i] = get_transaction( fbs.blk_state.signed_transactions[i] );
    }
    return fbs;
}
int32_t node::get_head_block_index()const 
{ 
    return int(my->m_block_chain.size())-1; 
}

const block& node::head_block()const { return my->m_block_chain.back(); }
block node::get_block_by_index( uint32_t index )
{
    if( index < my->m_block_chain.size() )
        return my->m_block_chain[index];
    THROW_GPM_EXCEPTION( "Block index %1% is beyond known block chain size.", %index );
    return block(); // prevent warnings;
}

block_state node::get_block_state( const boost::rpc::sha1_hashcode& blk_state )
{
    if( !my->m_block_state_db )
        THROW_GPM_EXCEPTION( "Database not open." );

    boost::optional<block_state> bs = my->m_block_state_db->get(blk_state);
    if( !!bs )
        return *bs;
    THROW_GPM_EXCEPTION( "Unknown block state %1%", %(blk_state) );
}


bool node::can_register( const std::string& name )
{
    return my->m_gen_trx->get_name_index(name) == -1;
}
bool node::get_key_for_name( const std::string& name, public_key_t& pk )
{
    if( !my->m_state_db )
        THROW_GPM_EXCEPTION( "Database not open." );

    return my->m_gen_trx->get_public_key_t( name, pk );
}

public_key_t node::get_public_key_t( const std::string& name )
{
    if( !my->m_state_db )
        THROW_GPM_EXCEPTION( "Database not open." );

    public_key_t pk;
    if( !my->m_gen_trx->get_public_key_t( name, pk ) )
        THROW_GPM_EXCEPTION( "No public key known for name '%1%'", %name );
    return pk;
}

bool node::has_balance( const std::string& account, const std::string& type )
{
    bool is_null;
    if( !my->m_state_db )
        THROW_GPM_EXCEPTION( "Database not open." );
    uint64_t b =  my->m_gen_trx->get_balance(account,type, &is_null);

    return !is_null;
}
uint64_t node::get_balance( const std::string& account, const std::string& type )
{
    if( !my->m_state_db )
        THROW_GPM_EXCEPTION( "Database not open." );
    return my->m_gen_trx->get_balance(account,type);
}


std::vector<std::string> node::get_account_contents( const std::string& account )
{
    slog( "get contents %1%", account );
    std::vector<std::string> names =  my->m_gen_trx->get_account_contents(account);
    std::vector<std::string> n;n.resize( names.size() );
    for( uint32_t i = 0; i < names.size(); ++i )
        n[i] = names[i];
    return n;

}


std::vector<char>  node::get_state_chunk( uint32_t part, const boost::rpc::sha1_hashcode& hash )
{
    return std::vector<char>();
}

int node::add_transaction( const signed_transaction& tx )
{
    wlog(  "add trx %1%", boost::rpc::to_json(tx) );
    boost::rpc::sha1_hashcode h = boost::rpc::raw::hash_sha1(tx);
    for( uint32_t i = 0; i < 3; ++i )
    {
        boost::optional<signed_transaction > trx = my->m_trx_db->get(std::make_pair(i,h) );
        if( !!trx )
        {
            wlog(  "we already know about this transaction." );
            return -1;
        }
    }
    my->m_trx_db->set( std::make_pair( PENDING_TRX, boost::rpc::raw::hash_sha1(tx)), tx ); 
    new_transaction(tx);
    if( is_generating() )
        my->start_block();
    return 0;
}

int node::add_full_block( const full_block_state& blk )
{
    bool added_trx = false;
//    slog( "blk.trxs.size %1%",  %boost::rpc::to_json(blk));

    for( uint32_t i = 0; i < blk.trxs.size(); ++i )
    {
        bool found = false;
        boost::rpc::sha1_hashcode h = boost::rpc::raw::hash_sha1(blk.trxs[i]);
        for( uint32_t x = 0; x < 3; ++x )
        {
            boost::optional<signed_transaction > trx = my->m_trx_db->get(std::make_pair(x,h) );
            if( !!trx )
            {
                wlog(  "we already know about this transaction." );
                found = true;
                x = 4;
            }
        }
        if( !found )
        {
            slog( "adding trx %1%", blk.blk_state.signed_transactions[i] );
            boost::rpc::sha1_hashcode hc = boost::rpc::raw::hash_sha1(blk.trxs[i]); 
            slog( "hc: %1%", hc );

            my->m_trx_db->set( std::make_pair( PENDING_TRX, hc), blk.trxs[i] ); 
            added_trx = true;
        }
    }
    std::vector<signed_transaction> strxs = my->get_transactions(blk.blk_state.signed_transactions, PENDING_TRX );
    my->m_block_state_db->set( blk.blk.state, blk.blk_state );


    if( my->m_block_chain.size() == 0 && blk.index == 0 )
    {
        if( !my->m_head_trx->apply( blk.blk, blk.blk_state.generator_name, 
            std::vector<signed_transaction>(), blk.blk_state.state_db ) )
        {
            elog( "Error applying first block." );
        }
        my->m_block_state_db->set( blk.blk.state, blk.blk_state );
        my->m_block_chain.push_back(blk.blk);
        return 1;
    }

    int rtn = add_block( blk.blk );
    if( rtn <= 0 && added_trx && is_generating() )
        my->start_block();
    if( rtn > 0 )
    {
        my->dump(my->m_block_chain);
    }

    return rtn;
}

// this method may yield if the block state for blk is not known...
int node::add_block( const block& blk )
{
    if( !my->m_block_chain.size() )
    {
        elog( "block chain size 0" );
        return 0;
    }

    boost::rpc::sha1_hashcode head_hash = boost::rpc::raw::hash_sha1(my->m_block_chain.back() );

    // this method may yield waiting for the result... 
    boost::optional<block_state> bs = my->find_block_state( blk.state );
    if( !bs ) 
    {
        elog( "unable to find block state %1%", blk.state );
        return -1;
    }

    uint32_t ct =gpm::utc_clock() / 1000000;
    if( blk.utc_time > (ct + 30)  )
    {
        elog( "Received block (prev:%3%) from %4% seconds in the future! block time %1%  ct: %2% ", 
                   blk.utc_time, ct, blk.prev_block, (blk.utc_time - ct));
        return -1;
    }
    
    if( blk.prev_block == head_hash )
    {
        my->m_block_chain.push_back(blk);
        if( validate( my->m_block_chain ) )
        {
            state_database_transaction::ptr trx( new state_database_transaction( my->m_head_trx ) );


            std::vector<signed_transaction> strxs = 
                my->get_transactions((*bs).signed_transactions, PENDING_TRX );
            if( strxs.size() != bs->signed_transactions.size() )
            {
                elog( "Unable to find all required transactions in the pending state." );
            }
            else if( trx->apply( blk, (*bs).generator_name, strxs, (*bs).state_db ) )
            {
                my->move_transactions( HEAD_TRX, APPLIED_TRX );
                my->move_transactions((*bs).signed_transactions, PENDING_TRX, HEAD_TRX );

                // write the old head to disk
                my->m_state_db->commit();
                // write head trx to file cache
                my->m_head_trx->commit();
                my->m_head_trx = trx;
                my->m_head_trx->rebase( my->m_state_db );


                my->m_gen_trx  = state_database_transaction::ptr( new state_database_transaction( my->m_head_trx ) );



                // save everything upto the last head
                my->m_block_chain.pop_back();
                save( my->m_datadir / "blockchain", my->m_block_chain );
                my->m_state_db->commit();
                my->m_trx_db->sync();
                my->m_block_state_db->sync();
                my->m_block_chain.push_back(blk);

                full_block_state fbs;
                fbs.index = my->m_block_chain.size()-1;
                fbs.head_index = my->m_block_chain.size()-1;
                fbs.trxs = strxs;
                fbs.blk = blk;
                fbs.blk_state = *bs;

                //emit
                new_block( fbs );

                if( my->m_gen_enabled )
                    my->start_block();
                return 1;
            }
        }
        elog( "blk: %1% %2%", boost::rpc::to_json(blk),  bs->generator_name  );
        my->dump( my->m_block_chain, my->m_block_chain.size()-10, 11);
        
        my->m_block_chain.pop_back();
        elog( "Error applying transactions." );
        return -3;
    }
    else if( blk.prev_block == my->m_block_chain.back().prev_block )
    {
        block old_head = my->m_block_chain.back();
        my->m_block_chain.back() = blk;
        if( validate( my->m_block_chain ) )
        {
            state_database_transaction::ptr trx( new state_database_transaction( my->m_state_db ) );

            std::vector<boost::rpc::sha1_hashcode> old_head_trx = my->get_head_transactions();
            my->move_transactions( old_head_trx, HEAD_TRX, PENDING_TRX );


            std::vector<signed_transaction> strxs = my->get_transactions((*bs).signed_transactions, PENDING_TRX);
            if( strxs.size() != bs->signed_transactions.size() )
            {
                elog( "Unable to find all required transactions in the pending state." );
            }
            else if( trx->apply( blk, (*bs).generator_name, strxs, (*bs).state_db ) )
            {
                my->move_transactions((*bs).signed_transactions, PENDING_TRX, HEAD_TRX );
                my->m_head_trx = trx;
                my->m_gen_trx   = state_database_transaction::ptr( new state_database_transaction( my->m_head_trx ) );

                full_block_state fbs;
                fbs.index = my->m_block_chain.size()-1;
                fbs.head_index = my->m_block_chain.size()-1;
                fbs.trxs = strxs;
                fbs.blk = blk;
                fbs.blk_state = *bs;
                // emit
                new_block( fbs );
                if( my->m_gen_enabled )
                    my->start_block();
                return 1;
            }
            else
                elog( "Error applying transactions." );
            my->move_transactions( old_head_trx, PENDING_TRX, HEAD_TRX );
        }
        else
            elog( "Error validating chain." );
        my->m_block_chain.back() = old_head;
        return -3;
    }
    else
    {
        elog( "Block.prev_block %1%  != head.prev_block (%2%) or head %3%",
                blk.prev_block,  my->m_block_chain.back().prev_block,  boost::rpc::raw::hash_sha1(my->m_block_chain.back()) );
    }
    return -1;
}

void node::dump( uint32_t start, uint32_t len )
{
    my->dump( my->m_block_chain, start, len );
}

void node_private::dump( const signed_transaction& trx )
{
    static boost::posix_time::ptime epoch(boost::gregorian::date(1970, boost::gregorian::Jan, 1));
     std::cerr << "     ";
    std::cerr << (epoch + boost::posix_time::microseconds(trx.trx.utc_time) ) << "  \n";
    for( uint32_t c = 0; c < trx.trx.commands.size(); ++c )
    {
        if( trx.trx.commands[c].id == cmd::register_name::id )
        {
            cmd::register_name rn = trx.trx.commands[c];
            std::cerr<<"        register_name: '" << rn.name <<"'  public_key: "<< boost::rpc::super_fast_hash(boost::rpc::to_json(rn.pub_key)) << std::endl;
        }
        else if( trx.trx.commands[c].id == cmd::issue::id )
        {
            cmd::issue rn = trx.trx.commands[c];
            std::cerr<<"        issue "<< rn.stock_name << std::endl;
        }
        else if( trx.trx.commands[c].id == cmd::transfer::id )
        {
            cmd::transfer tr = trx.trx.commands[c];
            std::cerr<<"        transfer: '" << std::dec << tr.amount <<"'  from: "<< (tr.from_name.size() ? tr.from_name : "*NEW*" )
                     <<"  to: " << (tr.to_name.size() ? tr.to_name : "(gen)") << std::endl;
        }
        else
        {
            std::cerr<<"        cmd: " << trx.trx.commands[c].id << std::endl;
        }
    }

}
void node_private::dump( const block_chain& bc, uint32_t start, uint32_t len )
{
    try  {
    slog( "dump block chain size %1%", bc.size() );
    static boost::posix_time::ptime epoch(boost::gregorian::date(1970, boost::gregorian::Jan, 1));
    for( uint32_t i =  start; i < bc.size() && i < start + len; ++i )
    {
        std::cerr << std::dec << i << "]   " << boost::rpc::raw::hash_sha1(bc[i]) 
                  << "  prev:  " << bc[i].prev_block 
                  << "  time:  " << (epoch + boost::posix_time::seconds( bc[i].utc_time )) 
                  << "  state: " << bc[i].state ;


        block_state bs = self->get_block_state( bc[i].state );
        std::cerr << "  state_db: " << bs.state_db << "  generator: " << bs.generator_name << std::endl;

        for( uint32_t t = 0; t < bs.signed_transactions.size(); ++t )
        {
            std::cerr<< "    " << t <<")  trx: " << bs.signed_transactions[t] << std::endl;
            signed_transaction trx = self->get_transaction( bs.signed_transactions[t] );
            dump(trx);
        }
    }
    }
    catch ( const boost::exception& e )
    {
    std::cerr<<"\n";
        elog( "exception: %1%", boost::diagnostic_information(e) );
    }
    std::cerr<<"\n";
}
std::vector<trx_log> node::get_transaction_log( const std::string& account, const std::string& type,
                                                          uint64_t start_date , uint64_t end_date )
{
    std::vector<trx_log> log;
    abstract_state_database::transfer tran = my->m_gen_trx->get_last_transfer( account, type );
    while( !tran.end() )
    {
        if( tran.time() < start_date )
            return log; 
        if( tran.time() <= end_date )
        {
            log.push_back( trx_log( tran.from(), tran.to(), tran.type(), tran.amount(), tran.to_balance(), tran.from_balance(), tran.time() ) );
        }
        tran.previous(account);    
    }
    return log;
}

} // namespace gpm

