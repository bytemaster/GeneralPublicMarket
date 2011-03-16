#include "block.hpp"
#include <boost/rpc/base64.hpp>
#include <boost/rpc/super_fast_hash.hpp>
#include <boost/rpc/json.hpp>

namespace gpm {

gpm::bigint to_bigint( const boost::rpc::sha1_hashcode& hc )
{
    return gpm::bigint( (const char*)hc.hash, sizeof(hc.hash) );
}

boost::rpc::sha1_hashcode calc_max_hash()
{
    boost::rpc::sha1_hashcode max_hash; memset( ((char*)max_hash.hash)+2, 0xff, 18 );
    return max_hash;
}
const boost::rpc::sha1_hashcode& get_max_hash()
{
    static boost::rpc::sha1_hashcode hc = calc_max_hash();
    return hc;
}

/**
 *  The probability of finding a hash 'H' less than some value 'V' in the range of 0 -> 'MAX'
 *
 *  V / MAX
 *
 *  The probability of finding a sequence of values Hi less than Vi is:
 *
 *  V1/MAX * V2/MAX * V3/MAX
 *
 *  The difficulty for any given node is MAX_TARGET / CURRENT_TARGET.
 *
 *  The CURRENT_TARGET is 1.001 * the maximum hash in BLOCK_WINDOW 
 *
 *  The BLOCK_WINDOW is the number of blocks generated per day.
 *
 *
 *  xf / y = df 
 *
 *
 */
int64_t calculate_difficulty( const block_chain& bc )
{
    int64_t d = 1;

    // we want .001 precision 
    static const gpm::bigint MAX_TARGET( to_bigint( get_max_hash() ) * gpm::bigint( 1000 ) );
    for( uint32_t i = 0; i < bc.size(); ++i )
    {
        // low hashes will be far from max
         int64_t c = 160 - (to_bigint( calculate_hash_target( bc, i ) )).log2();
         d += c;
    }
    return d;
}


/**
 *  The hash target is designed to estimate how low a hash value the cluster
 *  can calculate in a given block time interval.  It sets the threshold that
 *  must be crossed to add another block to the chain.  
 *
 *  Given all of the blocks generated in the past 24 hours, the target is the
 *  largest hash generated in the past 24 * 1.01.  This allows the hash target
 *  to get easier by 1% per block if there is a sudden drop in CPU power.
 */
boost::rpc::sha1_hashcode calculate_hash_target( const block_chain& bc, uint32_t index  )
{
    if( index == uint32_t(-1) )
        index = bc.size() - 1;

    if( index == 0 )
    {
        return get_max_hash();
    }

    boost::rpc::sha1_hashcode max = boost::rpc::raw::hash_sha1(bc[index]);

    int c = 1;
    for( uint32_t i = index; i > 0; --i )
    {
        if( bc[i].prev_block > max )
            max = bc[i].prev_block;
        if( ++c == BLOCK_WINDOW )
            break;
    }
    
    gpm::bigint m((const char*)max.hash, sizeof(max.hash) );
    gpm::bigint m1(m);
    m = ( m * gpm::bigint( 257 ) ) / gpm::bigint(256);
    std::vector<char> dat;
    boost::rpc::raw::pack(dat,m);

    if( dat.size() <= 20 )
    {
        memset( max.hash, 0, sizeof(max.hash) );
        memcpy( ((char*)max.hash) + 20 - dat.size(), &dat.front(), dat.size() );
    }
    else
    {
        wlog( "target greater than max  %1%", dat.size() );
    }
    if( max > get_max_hash() ) 
        return get_max_hash();
    return max;
}


/**
 *  Checks the following characteristics of the block chain:
 *      1) The prev_block == the hash of the previous block
 *      2) The time difference between the blocks is above the minimum
 *      3) The hash of a block is below the hash target for that block.
 *      4) There is at least one block.
 *
 */
bool validate( const block_chain& bc )
{
    if( bc.size() < 0 ) { return false; }

    uint32_t          prev_time = 0;
    boost::rpc::sha1_hashcode prev_hash;
    boost::rpc::sha1_hashcode cur_hash;
    boost::rpc::sha1_hashcode hash_target;

    for( uint32_t i = 0; i < bc.size(); ++i )
    {
        if( (bc[i].utc_time - prev_time ) < SECONDS_PER_BLOCK  )
        {
            wlog( "Not enough time elapsed between blocks, %1% seconds expected more than %2% ", 
                         (bc[i].utc_time - prev_time),
                         SECONDS_PER_BLOCK );
            return false;
        }

        if( bc[i].prev_block != prev_hash ) 
        {
            wlog( "Invalid previous hash %1% for block %2%  expected %3%", bc[i].prev_block, i,  prev_hash );

            wlog( "block[%1%] = %2% with hash %3%", i, boost::rpc::to_json( bc[i] ),  boost::rpc::raw::hash_sha1( bc[i] ) );
            wlog( "block[%1%] = %2% with hash %3%", (i-1), boost::rpc::to_json( bc[i-1] ),  boost::rpc::raw::hash_sha1( bc[i-1] ) );
            return false;
        }

        boost::rpc::raw::hash_sha1( bc[i], cur_hash );

        hash_target = calculate_hash_target( bc, i );
        if( cur_hash > hash_target )
        {
            wlog( "Hash %3% of block %1% is above target %2%", i, hash_target, cur_hash );
            return false;
        }
        prev_hash = cur_hash;
    }
    return true;
}

void dump( const block_chain& bc )
{
    for( uint32_t i = 0; i < bc.size(); ++i )
    {
        std::cerr<<i<<" " << boost::rpc::raw::hash_sha1( bc[i] ) << "] ";
        std::cerr<<bc[i].prev_block << "   " << bc[i].version << " " << bc[i].utc_time << "  " << bc[i].state << "\n";
    }
    slog( "Difficulty %1%",  calculate_difficulty(bc) );
    slog( "Valid: %1%", int( validate(bc) ) );
}


/**
 *  This method will attempt to find a nonce for block b that is less than target and
 *  will stop after 
 */
bool generate( block& b, const boost::rpc::sha1_hashcode target, uint64_t start, uint64_t stop  )
{
    //slog( "target %1%  nonce [%2% -> %3%]", %target  %start %stop);
    b.nonce = start;
    boost::rpc::sha1_hashcode h;
    boost::rpc::raw::hash_sha1( b, h );
    while( h > target && b.nonce < stop )
    {
        b.nonce++;
        boost::rpc::raw::hash_sha1( b, h );
    }
    return h < target;
}

uint64_t calculate_hash_per_sec( uint64_t hashes )
{
    boost::rpc::sha1_hashcode zero;
    block b;
    uint64_t start = 0;//usclock();
    generate( b, zero, 0, hashes );
    uint64_t end = 2000000; // usclock();
    
    return uint64_t(b.nonce / double((end-start)/1000000.0));
}

bool load( const boost::filesystem::path& p, block_chain& bc )
{
    if( ! boost::filesystem::exists( p ) ) 
        return false;

    FILE* f = fopen( p.native_file_string().c_str(), "rb" );
    if( f )
    {
        std::vector<char> dat;
        fseek( f, 0, SEEK_END );
        uint32_t size = ftell( f );
        fseek( f, 0, SEEK_SET );
        dat.resize(size);
        fread( &dat.front(), 1, dat.size(), f );
        boost::rpc::raw::unpack( dat, bc );
        fclose(f);
        return true;
    }
    return false;
}

bool              save( const boost::filesystem::path& p, const block_chain& bc )
{
    std::vector<char> data;
    boost::rpc::raw::pack( data, bc );

    dlog( "saving block chain size %1%", data.size() );
    gpm::bigint hc;

    FILE* f = fopen(p.native_file_string().c_str(), "wb" );
    if( !f ) return false;
    fwrite( &data.front(), 1, data.size(), f );
    fclose(f);
    return true;
}

std::string encode_address( const public_key_t& pub )
{
    boost::rpc::sha1_hashcode hc = boost::rpc::raw::hash_sha1(pub);
    hc.hash[0] = boost::rpc::super_fast_hash( (const char*)&hc.hash[1], sizeof(hc.hash)-4 );
    unsigned char* h = (unsigned char*)(hc.hash);
    h[0] = 0xff; // first 

    std::string en = boost::rpc::base64_encode( h, sizeof(hc.hash) );
    if( !validate_address_format(en) )
        return "ERROR";
    return en;
}

bool validate_address_format( const std::string& addr )
{
    std::string dec = boost::rpc::base64_decode( addr );
    if( dec.size() < 20 )
    {
        wlog( "decode size %1%", dec.size() );
        return false;
    }

    uint32_t sfh;
    memcpy(&sfh, dec.c_str(), sizeof(sfh) );
    uint32_t test = boost::rpc::super_fast_hash( dec.c_str()+4, 16 );
    char* h = (char*)&test;
    h[0] = 0xff;

    return sfh == test;
}



/**
 *   Given a public key, sha1 hash it.
 *
 *   Then calculate the super fast hash of the last 4 of 5 words of the hash and 
 *   use this for the first word.
 *
 *   Then replace the first byte with the 'version' of the address string.
 *
 *   Then encode using base 64
 */
std::string generate_address( public_key_t& pub, private_key_t& priv )
{
    generate_keys( pub, priv );
    return encode_address(pub);
}


} // namespace gpm
