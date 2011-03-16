#ifndef _GPM_COMMAND_HPP_
#define _GPM_COMMAND_HPP_
#include <boost/rpc/datastream/sha1.hpp>
#include <gpm/exception.hpp>
#include <gpm/crypto/crypto.hpp>

namespace gpm {

    using boost::rpc::sha1_hashcode;

    namespace cmd {
        /**
         *  We send the name in clear text so that we can enforce
         *  that only lower case names may be registered.  This is to prevent
         *  frauds such as one person registering apple and another Apple and another
         *  APPLE which may confuse people as to the true 'identity' of a name.
         */
        struct register_name
        {
            enum id_enum{ id = 0x01 };
            register_name( const std::string& n, const public_key_t& pk )
            :name(n),pub_key(pk){}
            register_name(){}

            // must only contain a-z0-9_@#$%&-+
            // max length 255, the name will be refered to by its
            // hash code in all other messages
            std::string  name;
            public_key_t   pub_key;
        };
        struct issue
        {
            enum id_enum{ id = 0x02 };
            issue( const std::string& n = "" ):stock_name(n){}
            std::string        stock_name;
        };
        
        struct transfer
        {
            enum id_enum{ id = 0x03 };
            uint64_t        amount;
            std::string     stock_name;
            std::string     from_name; // null -> new
            std::string     to_name; // null -> generator
        };

        struct delegated_transfer
        {
            enum id_enum{ id = 0x04 };
            uint64_t        amount;
            sha1_hashcode   stock_name;
            sha1_hashcode   from_name; // if 0, then source = new
            sha1_hashcode   to_name; // if 0, then dest == generator
            sha1_hashcode   signed_name; // must have delegated authority of from_name
        };
    };

    struct command
    {
        uint16_t          id;
        std::vector<char> data;
        command():id(0){}

        template<typename MsgType>
        command( const MsgType& m ):id(MsgType::id)
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
}

BOOST_REFLECT( gpm::command, BOOST_PP_SEQ_NIL,
    (id)
    (data)
)

BOOST_REFLECT( gpm::cmd::register_name, BOOST_PP_SEQ_NIL,
    (name)
    (pub_key)
)

BOOST_REFLECT( gpm::cmd::issue, BOOST_PP_SEQ_NIL,
    (stock_name)
)

BOOST_REFLECT( gpm::cmd::transfer, BOOST_PP_SEQ_NIL,
    (amount)
    (stock_name)
    (from_name)
    (to_name)
)

#endif
