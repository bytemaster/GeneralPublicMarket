#include <gpm/block_chain/block.hpp>
#include <gpm/time/usclock.hpp>
#include <boost/rpc/datastream/sha1.hpp>
#include <boost/rpc/json.hpp>

/**
 *  The purpose of this test is to validate the block chain
 *  processing functions.
 *
 */
int main( int argc, char** argv )
{
    gpm::block_chain bc(1);
    bc[0].utc_time = gpm::utc_clock() / 1000000;
/*
{
    fl::sha1_hashcode h;
    memset( ((char*)h.hash)+10, 0xff, 10 );

    char buf[20];
    memset( buf, 0xff, sizeof(buf) );
    static fl::bigint max((const char*)buf, sizeof(buf) );

    memset( buf, 0, sizeof(buf) );
    fl::bigint d = max;

    slog( "h %1%", %h );
    memcpy( buf+0, h.hash, 20 );
    for( uint32_t i = 0; i < 20; ++i )
    {
        d = (d * (max - fl::bigint( (const char*)buf, sizeof( buf )  ) ) ) / max;
        slog( "d %1%", %(max-d) );
    }

    uint32_t  max16 = 0xffff;
    uint32_t  half = 0x0fff;

    uint32_t m1 = (half * half) / max16;
    slog( "q %1%  h*h = %2%" , %m1 %(half*half));

    uint64_t t = 0x0ffffff;
    uint64_t m = 0x7ffffff;
    uint64_t p = m;

    for( uint32_t i = 0; i < 40; ++i )
    {
        p = (p * (m-t) ) / m;
        slog( "p %1%", %p );
    }

}
*/

    uint64_t hash_rate = gpm::calculate_hash_per_sec( 1000 * 1000 * 5  );
    slog( "hashrate: %1% / sec", hash_rate );

    // generate 10 blocks
    for( uint32_t i = 1; i < 1000; ++i )
    {
        bc.resize(bc.size()+1);
        boost::rpc::sha1_hashcode target = calculate_hash_target( bc, i ); 
        gpm::block best;
        best.nonce = 0;
        boost::rpc::raw::hash_sha1( bc[i-1], best.prev_block );
        bool found = false;
        do {
            best.utc_time = std::max( bc[i-1].utc_time + SECONDS_PER_BLOCK, uint32_t(gpm::utc_clock() / 100000) ); 
            if( best.utc_time != bc[i].utc_time ) best.nonce = 0;

            if( gpm::generate( best, target, best.nonce + 1, best.nonce + hash_rate ) )
            {
                found = true;
                bc[i] = best;
                boost::rpc::raw::hash_sha1( best, target );
                slog( "found hash %1%", target );
            }
        } while ( !found || (gpm::utc_clock() / 1000000) < bc[i].utc_time );
        slog( "%1%", boost::rpc::to_json(bc[i]) );
        slog( "Difficulty %1%", calculate_difficulty(bc) );
    }

    dump( bc );

}
