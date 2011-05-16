#ifndef _GPM_TRANSACTION_HPP_ 
#define _GPM_TRANSACTION_HPP_
#include <boost/rpc/datastream/datastream.hpp>
#include <gpm/block_chain/command.hpp>

namespace gpm {
   using boost::rpc::sha1_hashcode;
   /**
    *   A transaction is a list of commands that must all be executed in
    *   order or none get executed.
    */
   struct transaction
   {
        uint64_t                utc_time; 
        std::vector<command>    commands;   
   };

   /**
    *   Some commands within some transactions require a 
    *   signature of approval.  
    */
   struct signed_transaction
   {
        bool operator < ( const signed_transaction& strx )const
        {
            return trx.utc_time < strx.trx.utc_time;
        }
        transaction                   trx;
        std::vector<gpm::signature_t> sigs;
        void sign( const gpm::private_key_t& pk )
        {
            sha1_hashcode hash;
            boost::rpc::raw::hash_sha1( trx, hash );
            sigs.resize(sigs.size()+1);
            pk.sign( hash, sigs.back() );
        }
        bool verify( const gpm::public_key_t& pk )const
        {
            sha1_hashcode hash;
            boost::rpc::raw::hash_sha1( trx, hash );

            for( uint32_t i = 0; i < sigs.size(); ++i )
            {
                if( pk.verify( hash,sigs[i] ) )
                    return true;
            }
            return false;
        }
   };
}


BOOST_REFLECT( gpm::transaction, BOOST_PP_SEQ_NIL,
    (utc_time)
    (commands)
)

BOOST_REFLECT( gpm::signed_transaction, BOOST_PP_SEQ_NIL,
    (trx)
    (sigs)
)

#endif
