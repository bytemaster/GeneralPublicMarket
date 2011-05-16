#ifndef _GPM_BLOCK_HPP_
#define _GPM_BLOCK_HPP_
#include <boost/reflect/reflect.hpp>
#include <boost/rpc/datastream/sha1.hpp>
#include <gpm/math/bigint.hpp>
#include <boost/filesystem.hpp>
#include <gpm/block_chain/transaction.hpp>


// the block window prevents one lucky hash from
// greatly increasing the difficulty.
#define BLOCK_WINDOW        30
#define SECONDS_PER_BLOCK   15 

namespace gpm {
    using boost::rpc::sha1_hashcode;

    struct block
    {
        block():version(0),utc_time(0),nonce(0){}
        sha1_hashcode     prev_block;
        uint16_t          version;
        uint32_t          utc_time; 
        sha1_hashcode     state;
        uint64_t          nonce;
    };

    struct block_state
    {
        uint32_t                    index;
        std::string                 generator_name;
        sha1_hashcode               state_db;
        std::vector<sha1_hashcode>  signed_transactions;
    };

    /**
     *  This data structure defines all of the information about a block instead of 
     *  just the header / content.  
     */
    struct full_block_state
    {
        full_block_state(int32_t i=-1, uint32_t hi=-1)
        :index(i),head_index(hi){}

        int32_t                          index;
        int32_t                          head_index;
        block                            blk;
        block_state                      blk_state;
        std::vector<signed_transaction>  trxs;
    };

    typedef std::vector<block>             block_chain;
    typedef boost::shared_ptr<block_chain> block_chain_ptr;

    std::string               generate_address( public_key_t& pk, private_key_t& pk );
    std::string               encode_address( const public_key_t& pk );
    bool                      validate_address_format( const std::string& addr );


    uint64_t                  calculate_hash_per_sec( uint64_t hashes );
    int64_t                   calculate_difficulty( const block_chain& bc );
    gpm::bigint               to_bigint( const sha1_hashcode& hc );
    const sha1_hashcode&      get_max_hash();
    sha1_hashcode             calculate_hash_target( const block_chain& bc, uint32_t index = -1 );
    bool                      generate( block& b, const sha1_hashcode target, uint64_t start = 0, uint64_t stop = -1 );
    bool                      validate( const block_chain& bc );
    void                      dump( const block_chain& bc );
                              
    bool                      load( const boost::filesystem::path& p, block_chain& bc );
    bool                      save( const boost::filesystem::path& p, const block_chain& bc );


} // namespace gpm

BOOST_REFLECT( gpm::block, BOOST_PP_SEQ_NIL,
    (prev_block)
    (version)
    (utc_time)
    (state)
    (nonce)
)

BOOST_REFLECT( gpm::full_block_state, BOOST_PP_SEQ_NIL,
    (index)
    (head_index)
    (blk)
    (blk_state)
    (trxs)
)

BOOST_REFLECT( gpm::block_state, BOOST_PP_SEQ_NIL,
    (index)
    (generator_name)
    (state_db)
    (signed_transactions)
)


#endif
