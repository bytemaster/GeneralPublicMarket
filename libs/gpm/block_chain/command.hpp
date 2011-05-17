#ifndef _GPM_COMMAND_HPP_
#define _GPM_COMMAND_HPP_
#include <boost/rpc/datastream/sha1.hpp>
#include <gpm/exception.hpp>
#include <gpm/crypto/crypto.hpp>
#include <boost/rpc/raw.hpp>

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

        /**
         *  A bid is an offer to exchange a certain amount of one stock
         *  for a certain amount of another stock.
         *
         *  A bid is valid so long as it was signed by the offer account's 
         *  private key.  When the balance goes to 0, the bid is worthless.
         *
         *  To place a bid you would:
         *      create a new account (random name), transfer a multiple of offer_amount of offer_stock to account
         *      specify the amount of request stock you wish to exchage for the ammount of offer stock.
         *      Sign all values with the offer account private key
         *
         *  A bid is nothing without a counter-party
         */
        struct bid
        {
            std::string        offer_account;        
            std::string        offer_stock;
            uint64_t           offer_amount;
            std::string        request_stock;
            uint64_t           request_amount;
            sha1_hashcode      offer_account_signature;
        };

        /**
         *  If you have a signed bid with an available ballance then
         *  an accept_bid command can be used to fufill the order.
         *
         *  Assuming the command is signed by account, then
         *
         *  count * the_bid.request_amount of request_stock  will be transfered
         *  from account to offer account.
         *
         *  AND
         *
         *  count * the_bid.offer_amount of the_bid.offer_stock  will be transfered
         *  from the_bid.offer_account to account.
         *
         */
        struct accept_bid
        {
            enum id_enum{ id = 0x05 };
            bid             the_bid;
            std::string     account;
            uint64_t        count;
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
