#ifndef _BITBLOCKS_HPP_
#define _BITBLOCKS_HPP_
#include <gpm/exception.hpp>
#include <boost/rpc/raw.hpp>

using boost::rpc::sha1_hashcode;

struct create_account
{
    enum id_enum { id = 0x01 };
    std::string       recv_btc_address;  // account ID
};
BOOST_IDL_REFLECT( create_account, BOOST_PP_SEQ_NIL, (recv_btc_address) )

struct account_created
{
    enum id_enum { id = 0x02 };
    std::string       recv_btc_address;  // account ID
    std::string       pay_btc_address;   // addres to fund
};
BOOST_IDL_REFLECT( account_created, BOOST_PP_SEQ_NIL, (recv_btc_address)(pay_btc_address) )

// close account and refund balance
struct close_account
{
    enum id_enum { id = 0x03 };
};
BOOST_IDL_REFLECT(close_account, BOOST_PP_SEQ_NIL, BOOST_PP_SEQ_NIL )

struct query_server_status
{
    enum id_enum { id = 0x04 };
};
BOOST_IDL_REFLECT(query_server_status, BOOST_PP_SEQ_NIL, BOOST_PP_SEQ_NIL )

struct report_server_status
{
    enum id_enum { id = 0x05 };
    std::string pay_btc_address;   // address to fund
    uint64_t    min_price_per_kb;  // #BTC required per kb download
    uint64_t    avg_price_per_kb;  // Average priority, average price paid per kb
    uint64_t    max_price_per_kb;  // Maximum price paid per kb
    uint64_t    balance;           // Available Balance
};
BOOST_IDL_REFLECT( report_server_status, BOOST_PP_SEQ_NIL, 
    (pay_btc_address)
    (min_price_per_kb)
    (avg_price_per_kb)
    (max_price_per_kb)
    (balance)
)

struct request_bitblock
{
   enum id_enum { id = 0x06 };
   uint64_t      query_time;  // time request  was made
   uint64_t      btc_offer;  // amount willing to pay (priority), deducted from account
   sha1_hashcode block_hash;
   uint16_t      start_kb;
   uint16_t      num_kb;
};
BOOST_IDL_REFLECT( request_bitblock, BOOST_PP_SEQ_NIL, 
    (query_time)
    (btc_offer)
    (block_hash)
    (start_kb)
    (num_kb)
)

struct report_bitblock
{
   enum id_enum { id = 0x07 };
   uint64_t          query_time; // time from request
   sha1_hashcode     block_hash; // total block hash, not just hash for this kb
   uint64_t          min_price_per_kb;  // #BTC required per kb download
   uint64_t          balance;
   uint64_t          index_kb; 
   std::vector<char> data;
};
BOOST_IDL_REFLECT( report_bitblock, BOOST_PP_SEQ_NIL, 
    (query_time)
    (block_hash)
    (min_price_per_kb)
    (balance)
    (index_kb)
    (data)
)

/**
 *  Request that the server download a block from the client and make
 *  an offer for how much to pay.
 */
struct upload_bitblock
{
   enum id_enum { id = 0x08 };
   uint64_t          btc_offer;
   sha1_hashcode     block_hash;
   uint32_t          block_size;
};

BOOST_IDL_REFLECT( upload_bitblock, BOOST_PP_SEQ_NIL, 
    (btc_offer)
    (block_hash)
    (block_size)
)


/**
 *  Send request and include the sent_time
 *  Respond to the request by copying sent time to echo sent time
 *
 *  Keep increasing request rate until latency gets too high, then 
 *  back off.
 */
struct message
{
    uint8_t           id;
    std::vector<char> data;

    message():id(0){}

    template<typename MsgType>
    message( const MsgType& m ):id(MsgType::id)
    {
        boost::rpc::raw::pack( data, m );
    }

    template<typename MsgType>
    operator MsgType ()const 
    { 
        if( id != MsgType::id ) 
            THROW_GPM_EXCEPTION( "Invalid attempt to cast command to %1%", 
                                 %boost::reflect::get_typeinfo<MsgType>::name() ); 
        MsgType m;
        boost::rpc::raw::unpack( data, m );
        return m;
    }
};

BOOST_IDL_REFLECT( message, BOOST_PP_SEQ_NIL, (id)(data) )


#endif
