#include "state_database.hpp"
#include <boost/rpc/json.hpp>
#include <boost/rpc/super_fast_hash.hpp>
#include <boost/foreach.hpp>

namespace gpm {

typedef state_database_transaction sdt;

state_database_transaction::state_database_transaction( const abstract_state_database::ptr& new_base )
:base(new_base)
{
}
uint64_t sdt::size()const
{
    return start() + local_changes.size();
}
uint64_t  sdt::start()const 
{
    if( base )  
        return base->size();
    return 0;
}
uint64_t  sdt::get_record( uint64_t loc, state_record& r )
{
    if( loc == -1 )
        return 0;

    //slog( "start %1%", %start() );
    if( loc < start() )
    {
        if( base ) 
            return base->get_record(loc, r);
        elog( " err " );
        return 0;
    }
    loc -= start();
    if( local_changes.size() <= loc )
    {
        elog( "Invalid location %1%.", (loc + start()) );
        return 0;
    }
    boost::rpc::raw::unpack(  &local_changes[loc], local_changes.size()-loc, r );
    return boost::rpc::raw::packsize(r);
}

bool sdt::append_record(  const state_record& r )
{
    state_record r2(r); r2.previous = size();
    std::vector<char> v;
    boost::rpc::raw::pack( v, r2 );
    local_changes.insert( local_changes.end(), v.begin(), v.end() );
    wlog( "VVVVVVVV APPEND at %1% VVVVVVVV", r2.previous );
    dump(r);
    wlog( "^^^^^^^ APPEND ^^^^^^^^^" );
    return true;
}
bool  sdt::append( std::vector<char>& d, const account_index& idx, const name_index& nidx ) 
{
    local_changes.insert( local_changes.end(), d.begin(), d.end() );
    account_index::const_iterator itr = idx.begin();
    while( itr != idx.end() )
    {
        last_transfer_map[itr->first] = itr->second;
        ++itr;
    }
    name_index::const_iterator nitr = nidx.begin();
    while( nitr != nidx.end() )
    {
       last_name_edit_map[nitr->first] = nitr->second;
        ++nitr;
    }
    return true;
}

bool sdt::get_public_key_t( const std::string& name, public_key_t& pk )
{
    slog( "%1%", name );
    uint64_t nidx = get_last_name_edit_index(name);
    if( nidx == -1 )
    {
        wlog( "last_name_edit_map.find(%1%) == end", name );
        return false;
    }
    slog( "%1% found at %2%", name, nidx );
    state_record r;
    if( !get_record( nidx, r ) )
    {
        elog( "Error getting record at %1%", nidx );
        return false;
    }
//    slog( "record %1% at %2%", %boost::rpc::to_json(r) %nidx );

    if( r.id == define_name::id )
    {
        define_name dn = r;
        pk = dn.key;
        return true;
    }
    else if( r.id == update_name::id )
    {
        update_name un = r;
        pk = un.key;
        return true;
        /*
        get_record( un.name_idx, r );
        slog( "update name %1%", %boost::rpc::to_json(un) );
        slog( "name idx r %1%", %boost::rpc::to_json(r) );
        if( r.id == define_name::id )
        {
            define_name dn = r;
            slog( "define_name %1%", %boost::rpc::to_json(dn) );
            pk = dn.key;
            return true;
        }
        elog( "un.name_idx != define name" );
        return false;
        */
    }
    else
    {
        elog( "Unexpected record %1%", boost::rpc::to_json(r) );
    }
    return false;
}

std::string sdt::get_name_for_index( uint64_t idx )
{
    if( idx == -1 )
    {
        wlog( "No name at index %1%", idx );
        return std::string();
    }
    state_record r;
    if( !get_record( idx, r ) )
        THROW_GPM_EXCEPTION( "Error getting record at index %1%", %idx );

    if( r.id == define_name::id )
    {
        define_name dn = r;
        return dn.name;
    }
    else if( r.id == update_name::id )
    {
        update_name un = r;
        return get_name_for_index( un.name_idx );
    }
    THROW_GPM_EXCEPTION( "No name at index %1%", %idx );
    return std::string();
}
uint64_t sdt::get_name_index( const std::string& nidx )
{
    uint64_t lnidx = get_last_name_edit_index(nidx);
    if( lnidx == uint64_t(-1) )
        return -1;

    state_record r;
    if( !get_record( lnidx, r ) )
    {
        THROW_GPM_EXCEPTION( "Error getting record %1%", %lnidx );
    }

    if( r.id == define_name::id )
    {
//        slog( "name_idx %1%", %lnidx );
        return lnidx;
    }
    else if( r.id == update_name::id )
    {
        update_name un = r;
//        slog( "%1%", %boost::rpc::to_json(un) );
        return un.name_idx;
    }
    THROW_GPM_EXCEPTION( "Error getting name index for %1%", %nidx );
    return -1;
}


bool sdt::set_public_key( const std::string& name, const public_key_t& pk )
{
//    slog( "%1%", %name );
    name_index::iterator itr = last_name_edit_map.find(name);
    uint64_t lnidx = get_last_name_edit_index(name);
    if( lnidx == uint64_t(-1) )
    {
//        slog( "%1% defined at %2%", %name %size() );
        last_name_edit_map[name] = size();
        return append_record( define_name( name, pk ) );
    }

    state_record r;
    get_record( lnidx, r );

    if( r.id == define_name::id )
    {
        define_name dn = r;
//        slog( "update_name %1% ", %itr->second );
        last_name_edit_map[name] = size();
        return append_record( update_name( lnidx, pk, lnidx ) );
    }
    else if( r.id == update_name::id )
    {
        update_name un = r;
//        slog( "update name %1%", %boost::rpc::to_json( un ) );
        last_name_edit_map[name] = size();
        return append_record( update_name( un.name_idx, pk, lnidx ) );
    }
    elog( "Error setting public key for %1%", name );
    return false;
}

uint64_t sdt::get_balance( const std::string& account, const std::string& type, bool* is_null, uint64_t* loc  )
{
    uint64_t aidx = get_name_index(account);    
    uint64_t tidx = get_name_index(type);    
//    slog( "account idx: %1%   type index %2%", %aidx %tidx );

    uint64_t itr = get_last_transfer_index( account_key(aidx,tidx) );
    if( itr == uint64_t(-1) )
    {
        slog( "balance is NULL" );
        if( is_null ) 
            *is_null = true;
        if( loc ) 
            *loc = -1;
        return 0;
    }
    if( is_null ) 
        *is_null = false;

    if( loc ) 
        *loc = itr;

    state_record r;
    get_record( itr, r );
    if( r.id != transfer_log::id )
    {
        THROW_GPM_EXCEPTION( "Unexpected record type when looking for %1% : %2%", %account %type );
    }
    transfer_log tl = r;
//    slog( "tl %1%", %boost::rpc::to_json(tl) );
    if( tl.type_name != tidx )
        THROW_GPM_EXCEPTION( "Unexpected type when looking for %1% : %2%", %account %type );

    if( tl.from_name == aidx ) return tl.new_from_bal;
    if( tl.to_name == aidx ) return tl.new_to_bal;

    THROW_GPM_EXCEPTION( "Source/Dest Mismatch looking for %1% : %2%", %account %type );
    return 0;
}
void  sdt::transfer_balance( const std::string& from, const std::string& to, const std::string& type, uint64_t amnt, uint64_t trx_pos )
{
    slog( "transfer from %1% to %2%  %3% %4%", from, to, amnt, type );
    transfer_log  tl;
    tl.from_name = get_name_index(from);    
    tl.to_name   = get_name_index(to);    
    tl.type_name = get_name_index(type);    
    tl.amount    = amnt;

    tl.new_from_bal = get_balance( from, type, 0, &tl.last_from_trx );
    tl.new_to_bal   = get_balance( to, type, 0, &tl.last_to_trx );
    tl.trx_idx = trx_pos;
    wlog( "\n\n ==========>   tl trx_idx = %1%", tl.trx_idx );

    if( tl.new_from_bal < amnt )
        THROW_GPM_EXCEPTION( "Insufficent '%1%' in account '%2%',  '%3%' available, " 
                             "attempt to transfer %4% %1% to %5%", 
                                           %type %from  %tl.new_from_bal  %amnt %to );

    tl.new_from_bal -= amnt;
    tl.new_to_bal += amnt;

    uint64_t p = size();
    last_transfer_map[ account_key( tl.from_name, tl.type_name) ] = p;
    last_transfer_map[ account_key( tl.to_name, tl.type_name) ] = p;
    append_record( tl );
}
sdt::transfer sdt::get_last_transfer( const std::string& account, const std::string& type )
{
    return transfer( shared_from_this(), 
            get_last_transfer_index( account_key( get_name_index(account), get_name_index(type) ) ) );
}
void sdt::issue( const std::string& type, uint64_t trx_pos  )
{
    slog( "issue %1%", type ); 
    transfer_log  tl;
    tl.type_name = get_name_index(type);    
    tl.to_name   = get_name_index(type);    
    tl.from_name = get_name_index(type);    
    tl.amount       = uint64_t(-1);
    tl.new_from_bal = uint64_t(-1);
    tl.new_to_bal   = uint64_t(-1);
    tl.last_to_trx  = uint64_t(-1);
    tl.trx_idx = trx_pos;
        
    bool is_null = true;
    get_balance( type, type, &is_null, &tl.last_from_trx );
    if( !is_null )
        THROW_GPM_EXCEPTION( "Attempt to issue '%1%' after it has already been issued.", %type );

    uint64_t p = size();
    last_transfer_map[ account_key( tl.from_name, tl.type_name) ] = p;
    last_transfer_map[ account_key( tl.to_name, tl.type_name) ]   = p;
//    slog( "transfer_log: %1% at %2%", %boost::rpc::to_json(tl) %p);
    append_record( tl );
}



bool sdt::apply( const signed_transaction& trx, const std::string& gen_name )
{
    uint64_t trx_pos = size();
    wlog( "utc time %1%", trx.trx.utc_time );
    append_record( start_trx( boost::rpc::raw::hash_sha1( trx.trx),  trx.trx.utc_time ) );

    const std::vector<command>& cmds = trx.trx.commands;
    for( uint32_t i = 0; i < cmds.size(); ++i )
    {
        if( cmds[i].id == cmd::register_name::id )
        {
            slog( "register name?" );
            cmd::register_name rn = cmds[i];

            if( rn.name == encode_address( rn.pub_key ) )
            {
                set_public_key( rn.name, rn.pub_key );
            }
            else
            {
                public_key_t pub_key;
                if( get_public_key_t( rn.name, pub_key ) )
                {
                    slog( "verify" );
                    if( !trx.verify( pub_key ) )
                    {
                        elog( "Unable to apply transaction because it was not signed by current holder of name %1%", rn.name );
                        return false;
                    }
                    set_public_key( rn.name, rn.pub_key );
                }
                else
                {
                    slog( "set pub key" );
                    set_public_key( rn.name, rn.pub_key );
                }
            }
        }
        else if( cmds[i].id == cmd::issue::id )
        {
            cmd::issue is = cmds[i];
            public_key_t type_pub_key;

            if( !get_public_key_t( is.stock_name, type_pub_key ) )
            {
                elog( "No public key for name %1%", is.stock_name );
                return false;
            }
            bool is_null = false;
            uint64_t bal = get_balance( is.stock_name, is.stock_name, &is_null );
            if( !is_null )
            {
                elog( "stock already issued." );
                return false;
            }
            if( !trx.verify( type_pub_key ) )
            {
                elog( "Stock issue not signed by owner of '%1%", is.stock_name );
                return false;
            }
            issue( is.stock_name, trx_pos );
        }
        else if( cmds[i].id == cmd::transfer::id )
        {
             cmd::transfer tr = cmds[i];
             if( tr.to_name.size() && tr.from_name == tr.to_name )
             {
                elog( "Source account equals destinationa account." );
                return false;
             }
             public_key_t type_pub_key;
             public_key_t from_pub_key;
             if( !get_public_key_t( tr.from_name, from_pub_key ) )
             {
                 elog( "No public key for name %1%", tr.from_name );
                 return false;
             }
             if( !get_public_key_t( tr.stock_name, type_pub_key ) )
             {
                 elog( "No public key for name %1%", tr.stock_name );
                 return false;
             }
             if( !trx.verify( from_pub_key ) )
             {
                 elog( "Transfer not signed by source(%1%) private key.", tr.from_name );
                 return false;
             }
             
             uint64_t bal = get_balance( tr.from_name, tr.stock_name );
             if( bal < tr.amount )
             {
                 elog( "Insufficent %1% in account %2%", tr.stock_name, tr.from_name ); 
                 return false;
             }
             std::string to = tr.to_name;
             if( to.size() == 0 )
                 to = gen_name;
    
            
             uint64_t to_idx  = get_name_index(to);    
             if( to_idx == -1 )
             {
                // the destination is not a registered name...
                // is it a valid address?  else return false.
                if( validate_address_format(to) )
                {
                    wlog( "defining name of destination '%1%' to empty public key", to );
                    last_name_edit_map[to] = size();
                    append_record( define_name( to, public_key_t() ) );
                }
                else
                {
                    elog( "Destination name is not a registered name or valid address." );
                    return false;
                }
             }
             
             transfer_balance( tr.from_name, to, tr.stock_name, tr.amount, trx_pos );
        }
        else
        {
            elog( "Unknown command %1%", int(cmds[i].id) );
            return false;
        }
    }
    return true;
}

bool sdt::apply( const block& b, const std::string& gen, const std::vector<signed_transaction>& trx, const boost::rpc::sha1_hashcode& checksum  )
{
//    slog( "%4% apply %1%    start: %2%  size: %3%", %boost::rpc::to_json(b) %start() %size() %this);
    for( uint32_t i = 0; i < trx.size(); ++i )
    {
        if( !apply( trx[i], gen  ) )
            return false;
    }
    boost::rpc::sha1_hashcode check = calculate_state_hash();
    if( check != checksum )
    {
        dump( 1000 );
        elog( "Checksum of state failed, calculated %1%  expected %2%", check, checksum );
        return false;
    }
    append_record( end_block( b, gen ) );
//    slog( "start: %1%  size: %2%",  %start() %size() );
    return true;
}

/**
 *  @param pos - the position to start
 *  @param buf - where to read the data
 *  @param len - the number of bytes to read
 *  @return the number of bytes read
 */
 uint64_t sdt::read( uint64_t pos, char* buf, uint64_t len )
 {
    uint64_t r = 0;
    if( pos < start() )
    {
        r   += base->read( pos, buf, len );
        pos += r;
        buf += r;
        len -= r;
        if( len == 0 )
            return r;
    }
    if( pos < start() )
        THROW_GPM_EXCEPTION( "Invariant broken!" );
    
    if( local_changes.size() )
    {
        int av = std::min( len, uint64_t(local_changes.size()-(pos-start())) );
        memcpy( buf, &local_changes[pos-start()], av );
        r += av;
    }
    return r;
 }

/**
 *  For each MB of the log, calculate the hash.
 *  Then hash all of the hashes into one.  This will allow nodes to have
 *  sparce state information.  
 */
boost::rpc::sha1_hashcode sdt::calculate_state_hash()
{
    std::vector<char> tmp(1024*1024);
    std::vector<boost::rpc::sha1_hashcode> hashes;

    uint64_t pos = 0;
    uint64_t r   = read( pos, &tmp.front(), tmp.size() );
    while( r == tmp.size() )
    {
        boost::rpc::datastream<boost::rpc::sha1> ds;
        ds.write( &tmp.front(), r );
        hashes.push_back(ds.result());
        pos += r;
        uint64_t r   = read( pos, &tmp.front(), tmp.size() );
    }
    if( r )
    {
        boost::rpc::datastream<boost::rpc::sha1> ds;
        ds.write( &tmp.front(), r );
        hashes.push_back(ds.result());
    }
    slog( "size: %1% ", size() );
    
    return boost::rpc::raw::hash_sha1(hashes);
}


bool sdt::commit()
{
    slog( "commit %1%", this );
    if( base ) 
        base->append( local_changes, last_transfer_map, last_name_edit_map );
    local_changes.clear();
    last_transfer_map.clear();
    last_name_edit_map.clear();
    return true;
}
void sdt::abort()
{
    local_changes.clear();
    last_transfer_map.clear();
    last_name_edit_map.clear();
}

block sdt::find_last_block()
{
   slog( "size: %1%", size() );
   if( size() < 8 )
        return block();

   uint64_t last_record = size()-sizeof(uint64_t);
   read( last_record, (char*)&last_record, sizeof(last_record) );
   state_record r;
   get_record( last_record, r );
   while( r.id != end_block::id && r.previous != 0)
   {
        read( r.previous-8, (char*)&last_record, sizeof(last_record) );
        get_record( last_record, r );
   }
   if( r.id == end_block::id )
   {
        end_block eb = r;
        wlog( "last known block %1%", boost::rpc::to_json( eb )  );
        return eb.blk;
   }
   return block();
}

typedef state_database sd;


state_database::state_database()
{
}

bool sd::open( const boost::filesystem::path& file )
{
    namespace bfs = boost::filesystem;
    if( !boost::filesystem::exists(file) )
        bfs::create_directory( file );
    if( !boost::filesystem::is_directory(file) )
        THROW_GPM_EXCEPTION( "expected state database '%1%' to be a directory", %file );

    if( boost::filesystem::exists(file/"state") )
        m_file = gpm::file::ptr( new gpm::file( file/"state", "rb+" ) );
    else
        m_file = gpm::file::ptr( new gpm::file( file/"state", "wb+" ) );
    m_transfer_db.open( file/"transfer_index" );
    m_name_db.open( file/"name_index" );
    return true;
}

uint64_t sd::size()const
{
    return m_file->size() + local_changes.size();
}
uint64_t  sd::start()const 
{
    return 0;
}

uint64_t sd::read( uint64_t pos, char* buf, uint64_t len )
{
//    slog( "pos: %1%  size: %2%", %pos %len );
    uint64_t r = 0;
    if( pos < m_file->size() )
    {
        m_file->seek(pos);
        r += m_file->read( buf, len );
        len -= r;
        pos += r;
        buf += r;
    }
//    slog( "pos: %1%  size: %2%", %pos %len );
    if( len  && local_changes.size() )
    {
        uint64_t m =  std::min( uint64_t(local_changes.size()-(pos-m_file->size())),len);
        memcpy( buf, &local_changes[pos-m_file->size()], m );
        r += m;
    }
    return r;
}
uint64_t     sd::get_record( uint64_t loc, state_record& r )
{
    //slog( "%1%", %loc );
    r.id = 0;
    if( sizeof(r.id) != read( loc,(char*)&r.id, sizeof(r.id ) ) )
        return 0;
   // slog( "id: %1%", %r.id );
    uint32_t len = 0;
    if( sizeof(len) != read( loc + sizeof(r.id), (char*)&len, sizeof(len) ) )
        return 0;
   // slog( "len: %1%", %len );
    r.data.resize(len);
    if( len )
    {
        if( len != read( loc + sizeof(r.id) + sizeof(len), &r.data.front(), len ) )
            return 0;
    }
    if( sizeof(r.previous) == read( loc + sizeof(r.id) + sizeof(len) + len, (char*)&r.previous, sizeof(r.previous) ) )
        return sizeof(r.previous) + sizeof(r.id) + sizeof(len) + len;

    return 0;
}

bool      sd::commit()
{
    if( local_changes.size() )
    {
        m_file->write( &local_changes.front(), local_changes.size() );
        account_index::const_iterator itr = last_transfer_map.begin();
        while( itr != last_transfer_map.end() )
        {
            m_transfer_db.set( itr->first, itr->second );
            ++itr;
        }
        name_index::const_iterator nitr = last_name_edit_map.begin();
        while( nitr != last_name_edit_map.end() )
        {
            m_name_db.set( nitr->first, nitr->second );
            ++nitr;
        }
        m_transfer_db.sync();
        m_name_db.sync();
        local_changes.clear();
        last_transfer_map.clear();
        last_name_edit_map.clear();
    }
}


uint64_t sd::get_last_name_edit_index( const std::string& nidx )
{
//   slog( " '%1%'", %nidx );
    uint64_t idx = sdt::get_last_name_edit_index(nidx);
    if( idx == uint64_t(-1) )
    {
        boost::optional<uint64_t> i = m_name_db.get(nidx);
        if( !!i ) { return *i; }
        return -1;
    }
    return idx;
}

uint64_t  sdt::get_last_name_edit_index( const std::string& nidx )
{
     //slog( " '%1%'", %nidx );
     name_index::const_iterator itr = last_name_edit_map.find(nidx);
     if( itr == last_name_edit_map.end() ) 
     {
 //       slog( "trx does not have '%1%'", %nidx );
        if( base ) 
        {
  //          slog( "asking base '%1%'", %nidx );
            return base->get_last_name_edit_index(nidx);
        }
        return -1;
     }
    return itr->second;
}
uint64_t  sdt::get_last_transfer_index( const account_key& a )
{
     account_index::const_iterator itr = last_transfer_map.find(a);
     if( itr == last_transfer_map.end() ) 
     {
        if( base ) 
            return base->get_last_transfer_index(a);
        return -1;
     }
    return itr->second;
}


uint64_t  sd::get_last_transfer_index( const account_key& a )
{
    uint64_t idx = sdt::get_last_transfer_index(a);
    if( idx == uint64_t(-1) )
    {
        boost::optional<uint64_t> i = m_transfer_db.get(a);
        if( !!i ) { return *i; }
        return -1;
    }
    return idx;
}


typedef abstract_state_database::transfer tran;

tran::transfer( const abstract_state_database::ptr& p, uint64_t index  )
:asd(p),m_index(index)
{
    update();
};

bool tran::update()
{
    if( asd && (m_index != -1) )
    {
       state_record r;
       asd->get_record(m_index, r);
       if( r.id == transfer_log::id )
       {
            transfer_log tl = r;
            m_from_bal = tl.new_from_bal;
            m_to_bal   = tl.new_to_bal;
            m_amount   = tl.amount;
            m_from     = asd->get_name_for_index( tl.from_name );
            m_to       = asd->get_name_for_index( tl.to_name );
            m_type     = asd->get_name_for_index( tl.type_name );
            m_prev_from = tl.last_from_trx;
            m_prev_to   = tl.last_to_trx;

            state_record trxr;
            elog( "\n\n ==========>   tl trx_idx = %1%", tl.trx_idx );
            if( asd->get_record( tl.trx_idx, trxr ) )
            {
                if( trxr.id == start_trx::id )
                {
                    start_trx s = trxr;
                    m_time = s.utc_time;
                }
                else
                    m_time = 1000000ll*60*60*24;
            }
            else
                m_time = -1;
            return true;
       }
    }
    return false;
}


bool tran::previous( const std::string& account )
{
    if( m_to == m_from )
    {
        m_index = -1;
        return false;
    }
    if( account == m_to )
    {
        if( m_prev_to < m_index )
        {
            m_index = m_prev_to;
            return update();
        }
    }
    else if( account == m_from )
    {
        if( m_prev_from < m_index )
        {
            m_index = m_prev_from;
            return update();
        }
    }
    m_index = -1;
    return false; 
}

void sdt::dump( const state_record& r )
{
    std::cerr<< std::dec << "prev: "<<r.previous<<"  ";
    if( r.id == gpm::define_name::id )
    {
        gpm::define_name dn = r;
        std::cerr << "define " << dn.name << " =>  " << boost::rpc::super_fast_hash(boost::rpc::to_json(dn.key)) <<"\n";
    }
    if( r.id == gpm::update_name::id )
    {
        gpm::update_name dn = r;
        std::cerr << "update " << dn.name_idx << " =>  " << boost::rpc::super_fast_hash(boost::rpc::to_json(dn.key)) << "last update: " << dn.last_update_idx <<"\n";
    }
    if( r.id == gpm::transfer_log::id )
    {
        gpm::transfer_log tl = r;
        std::cerr << boost::rpc::to_json(tl) << std::endl;
    }
    if( r.id == gpm::start_trx::id )
    {
        gpm::start_trx tl = r;
        std::cerr << boost::rpc::to_json(tl) << std::endl;
    }
    if( r.id == gpm::end_block::id )
    {
        gpm::end_block tl = r;
        std::cerr << boost::rpc::to_json(tl) << std::endl;
    }
}
void sdt::dump(int max)
{
    state_record r;
    int c = 0;
    uint64_t p = 0;
    uint64_t len = get_record( p, r ); 
    while (len && c < max)
    {
        ++c;
        std::cerr<< std::dec << p << " ] prev: "<<r.previous<<"  ";
        if( r.id == gpm::define_name::id )
        {
            gpm::define_name dn = r;
            std::cerr << "define " << dn.name << " =>  " << boost::rpc::super_fast_hash(boost::rpc::to_json(dn.key)) <<"\n";
        }
        if( r.id == gpm::update_name::id )
        {
            gpm::update_name dn = r;
            std::cerr << "update " << dn.name_idx << " =>  " << boost::rpc::super_fast_hash(boost::rpc::to_json(dn.key)) << "last update: " << dn.last_update_idx <<"\n";
        }
        if( r.id == gpm::transfer_log::id )
        {
            gpm::transfer_log tl = r;
            std::cerr << boost::rpc::to_json(tl) << std::endl;
        }
        if( r.id == gpm::start_trx::id )
        {
            gpm::start_trx tl = r;
            std::cerr << boost::rpc::to_json(tl) << std::endl;
        }
        if( r.id == gpm::end_block::id )
        {
            gpm::end_block tl = r;
            std::cerr << boost::rpc::to_json(tl) << std::endl;
        }
        p += len;
        len = get_record( p, r ); 
    }
}

std::vector<std::string> sdt::query_names( const std::string& start, const std::string& end, uint32_t limit  )
{
    std::vector<std::string> names;
    if( base ) 
        names = base->query_names(start,end,limit);

    name_index::const_iterator s = last_name_edit_map.lower_bound(start);
    name_index::const_iterator e = last_name_edit_map.upper_bound(end);
    while( s != e ){  names.push_back(s->first); ++s; }
    if( names.size() > limit ) 
        names.resize(limit);

    std::sort(names.begin(),names.end());
    names.erase( std::unique( names.begin(), names.end() ), names.end() );
    return names;
}
std::vector<std::string> sd::query_names( const std::string& start, const std::string& end, uint32_t limit  )
{
    std::vector<std::string> names;
    if( base ) 
        names = base->query_names(start,end,limit);

    name_index::const_iterator s = last_name_edit_map.lower_bound(start);
    name_index::const_iterator e = last_name_edit_map.upper_bound(end);
    while( s != e ){  names.push_back(s->first); ++s; }

    bdb::keyvalue_db<std::string,uint64_t>::iterator itr = m_name_db.search(start);
    while( !itr.end() && itr.key() <= end )
    {
        names.push_back( itr.key() );
        if( names.size() > limit )
            return names;
        ++itr;
    }
    std::sort(names.begin(),names.end());
    names.erase( std::unique( names.begin(), names.end() ), names.end() );

    return names;
}

uint64_t sdt::name_count()
{
    int count = 0;
    if( base ) count = base->name_count();

    name_index::const_iterator itr = last_name_edit_map.begin();
    while( itr != last_name_edit_map.end() )
    {
        if( base->get_name_index(itr->first) == -1 )
            ++count;
        ++itr;
    }
    return count;
}
uint64_t sd::name_count()
{
    int count = m_name_db.count();

    name_index::const_iterator itr = last_name_edit_map.begin();
    while( itr != last_name_edit_map.end() )
    {
        if( m_name_db.find(itr->first).end() )
            ++count;
        ++itr;
    }
    return count;
}

std::vector<uint64_t>  sdt::get_account_contents_idx( uint64_t acnt_idx )
{
    std::vector<uint64_t> cts;

    account_key lak( acnt_idx,   0 ); 
    account_key uak( acnt_idx+1, 0 ); 

    account_index::const_iterator s = last_transfer_map.lower_bound(lak);
    account_index::const_iterator e = last_transfer_map.upper_bound(uak);
    while( s != e ){  cts.push_back( s->first.type_name ); ++s; }

    if( base ) 
    {
        std::vector<uint64_t> bcts(base->get_account_contents_idx(acnt_idx) );
        cts.insert(cts.end(), bcts.begin(), bcts.end() );
    }
    return cts;
}
std::vector<std::string>  sdt::get_account_contents( const std::string& acnt )
{
    slog( "get account contents %1%", acnt );
    std::vector<std::string> names;
    if( base ) 
        names = base->get_account_contents( acnt );
    
    account_key lak( get_name_index(acnt),   0 ); 
    account_key uak( get_name_index(acnt)+1, 0 ); 


    account_index::const_iterator s = last_transfer_map.lower_bound(lak);
    account_index::const_iterator e = last_transfer_map.upper_bound(uak);
    while( s != e ){  names.push_back( get_name_for_index(s->first.type_name) ); ++s; }

    std::sort(names.begin(),names.end());
    /*
    BOOST_FOREACH( const std::string& s, names )
    {
        slog( "found in trx %1%", %s );
    }
    */
    names.erase( std::unique( names.begin(), names.end() ), names.end() );
    return names;
}
std::vector<uint64_t>  sd::get_account_contents_idx( uint64_t acnt_idx )
{
    std::vector<uint64_t> cts = sdt::get_account_contents_idx(acnt_idx);
    account_key ak( acnt_idx, 0 ); 
    
   bdb::keyvalue_db<account_key,uint64_t>::iterator itr = m_transfer_db.search(ak);
   while( !itr.end() )
   {
        if( itr.key().account_name == ak.account_name )
        {
            cts.push_back( itr.key().type_name );
        }
        else
        {
            std::sort(cts.begin(),cts.end());
            cts.erase( std::unique( cts.begin(), cts.end() ), cts.end() );
            return cts;
        }
        ++itr;
   }
   return cts;
}
std::vector<std::string>  sd::get_account_contents( const std::string& acnt )
{
//   slog( "query %1%", %acnt );
   std::vector<std::string> names = sdt::get_account_contents(acnt);
   account_key ak( get_name_index(acnt), 0 ); 
//   slog( "%1%  keys %2%", %get_name_index(acnt)  %m_transfer_db.count() ); 
   bdb::keyvalue_db<account_key,uint64_t>::iterator itr = m_transfer_db.search(ak);
   while( !itr.end() )
   {
        if( itr.key().account_name == ak.account_name )
        {
            names.push_back( get_name_for_index( itr.key().type_name ) );
        }
        else
        {
            std::sort(names.begin(),names.end());
            names.erase( std::unique( names.begin(), names.end() ), names.end() );
            return names;
        }
        ++itr;
   }
    return names;
}

/**
 *  Parses the file and updates the index in the event that data was lost.
 */
void sd::update_index( )
{

}


} // namespace gpm

