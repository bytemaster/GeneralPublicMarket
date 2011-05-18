#ifndef _BLOCK_HPP_
#define _BLOCK_HPP_
#include <boost/reflect/reflect.hpp>
#include <bitblocks/keyvalue_db.hpp>

using boost::rpc::sha1_hashcode;

struct block
{
    uint64_t          publish_date;
    uint64_t          last_request;
    uint64_t          total_btc;
    uint64_t          total_requests; // total_btc/total_request = avg BTC/req
    std::string       payto_address; 
    std::vector<char> data;
};

struct user_id
{
    uint32_t ip;
    uint16_t port;

    bool operator > ( const user_id& r ) const {
        return ip > r.ip  || (ip == r.ip &&  port > r.port );
    }
    bool operator == ( const user_id& r ) const {
        return ip == r.ip  && ip == r.ip;
    }
};

struct metafile {
    metafile( const std::string& n = "" ):name(n){}
    std::string name;
    uint64_t    size;
    std::vector<sha1_hashcode> blocks;
};
BOOST_REFLECT( metafile, BOOST_PP_SEQ_NIL, (name)(size)(blocks) )

struct user_info
{
    uint64_t    created_utc;
    uint64_t    last_recv_utc;

    std::string payto_address;   // send money to user
    std::string receive_address; // receive money from user

    uint64_t    total_received;  // total money received at receive_address
    uint64_t    total_spent;     // total money consumed 
    uint64_t    total_earned;    // total money earned, but not yet sent
    uint64_t    total_paid;      // total money sent to payto address

    int64_t     balance()const { return  total_received + total_earned - total_spent - total_paid; } 
};


BOOST_REFLECT( block, BOOST_PP_SEQ_NIL, 
    (publish_date)
    (last_request)
    (total_btc)
    (total_requests)
    (payto_address)
    (data)
)

BOOST_REFLECT( user_id, BOOST_PP_SEQ_NIL, (ip)(port) )

BOOST_REFLECT( user_info, BOOST_PP_SEQ_NIL, 
    (created_utc)
    (last_recv_utc)
    (total_received)
    (total_spent)
    (total_earned)
    (total_paid)
    (payto_address)
    (receive_address)
)


typedef bdb::keyvalue_db<user_id,user_info>   user_db;
typedef bdb::keyvalue_db<sha1_hashcode,block> block_db;

#endif
