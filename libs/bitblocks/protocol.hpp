#ifndef _BITBLOCKS_PROTO_HPP_
#define _BITBLOCKS_PROTO_HPP_
#include <gpm/exception.hpp>
#include <boost/rpc/raw.hpp>
namespace proto {

    struct message
    {
        uint16_t                     id;
        std::vector<char>            data;

        message():id(0){}

        template<typename MsgType>
        message( const MsgType& m )
        :id(MsgType::id)
        {
            boost::rpc::raw::pack( data, m );
        }
        template<typename MsgType>
        operator MsgType ()const 
        { 
            if( id != MsgType::id ) 
                THROW_GPM_EXCEPTION( "Invalid attempt to cast command to %1%", %boost::reflect::get_typeinfo<MsgType>::name() ); 
            MsgType m;
            boost::rpc::raw::unpack( data, m );
            return m;
        }
    };

    struct create_account
    {
        enum id_enum { id = 0x01 };
        create_account( const std::string& btc_addr = std::string() )
        :btc_address(btc_addr){}

        std::string       btc_address;  // account ID
    };

    struct store_block
    {
        enum id_enum { id = 0x02 };
        store_block( const std::vector<char>& d = std::vector<char>(),
                     const std::string& pta = "" )
                     :payto_address(pta),data(d){}
        std::string       payto_address;
        std::vector<char> data;
    };

    struct request_block
    {
        enum id_enum { id = 0x03 };
        request_block( int64_t id = 0, const sha1_hashcode& hc = sha1_hashcode(), int64_t s = 0, uint64_t l = -1)
        :request_id(id),block_id(hc),start(s),len(l){}

        int64_t       request_id;
        sha1_hashcode block_id;
        int64_t       start;
        uint64_t      len;
    };

    struct report_block
    {
        enum id_enum { id = 0x04 };
        report_block(){}
        report_block( int64_t rid, int64_t s, const std::vector<char>& d = std::vector<char>() )
        :request_id(rid),start(s),data(d){}
        int64_t           request_id;
        int64_t           start;
        std::vector<char> data;
    };

} // namespace proto

BOOST_REFLECT( proto::create_account, BOOST_PP_SEQ_NIL, (btc_address) )
BOOST_REFLECT( proto::store_block,    BOOST_PP_SEQ_NIL, (payto_address)(data) )
BOOST_REFLECT( proto::request_block,  BOOST_PP_SEQ_NIL, (request_id)(block_id)(start)(len) )
BOOST_REFLECT( proto::report_block,   BOOST_PP_SEQ_NIL, (request_id)(start)(data) )
BOOST_REFLECT( proto::message,        BOOST_PP_SEQ_NIL, (id)(data) )

#endif // _BITBLOCKS_PROTO_HPP_
