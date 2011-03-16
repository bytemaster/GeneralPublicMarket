#ifndef _BLOCK_CHAIN_HPP_
#define _BLOCK_CHAIN_HPP_

namespace gpm {

    /**
     *  The block chain is kept to the minimum data required
     *  to know the state of the chain.  
     *
     *  The block chain simply verifies the state of the name and
     *  account databases at a given point of time.  It includes a
     *  hash of the transactions required to make the change to
     *  the account database.
     *
     *  92 bytes per block, 1 block every 5 minutes.  26kb/year.  
     *
     *  Once the longest block is found, then the name database
     *  is downloaded.  At 100kb/sec 200, you can download 200 
     *  names per second.  
     *
     *  Lowest hash for any block is the winner regardless of
     *  what time it came in.  A replacement for the head will be
     *  accepted as long as it is lower than the current head once
     *  a node is no longer the head it cannot be replaced except
     *  by a longer chain.
     *
     *  Hash must be lower than the median of the past 1024 hashes.  This
     *  provides an automatic lower bound to the difficulty.  
     *
     *  In this way the difficulty is automatically managed and no one
     *  wastes time one a split blockchain due to race conditions between
     *  when completed nodes are sent out to the network.
     *
     *  Upon receiving a block with a lower hash, the node will request the transactions 
     *  of that block, verify and apply the transactions to the current state, and
     *  then check that the new hash of the state matches the hash of the block.
     *
     *  Each node maintains a database of applied transactions and unapplied transactions.
     *
     *  The names_hash and accounts_hash are for a file containing the 1 hash per 1MB chunk.
     *  This allows users to only download the parts of the database they care about. They can then
     *  query other nodes for the index of a name or account and then download that one part to
     *  satisfy their 'queries'.   To lookup an account balance would require the following:
     *      
     *
     *      Request namefile hash index. 
     *      Request Name File Index for "name" from node with complete DB
     *      Request block of namefile containing "name"
     *          - validate hash, validate that 'name' is stored at index.
     *
     *      Request Account DB file index for Name / Stock
     *      Request chunk, validate hash, read index to verify balance.
     *
     *      
     *   Account Name       
     *  
     *  
     *  Permission System
     *      Normal transactions cannot be reversed where as permission transactions can be reversed as
     *      they represent 'delegated' authority.  
     *
     *      A permision is a 'name' that can be given to any other name in an 'amount' if it is 
     *          A:  Signed by 'permision_name'
     *          B:  Signed by anyone who is in 'permision_name' group with grant permission
     *          C:  The signed name has 'permision amount' greater than 'amount' or belongs to a group, directly or indirectly
     *                  who has that permission amount.
     *
     *      A permission may be removed by anyone in permision_name group who has more 'permission_name' than
     *          the person they are removing it from and has 'remove' priv.
     *
     *      A name may be added / removed from a group by:
     *          'group' and may delegate whether that group member has add, remove, and grant privs on the group. 
     *
     *      Any permissions given to 'name' are inherited by everyone in 'name'.  
     *
     *      Create a graph name <-> group <-> group
     *
     *          For my name, check to see if I have permission, if not then do a breth first search
     *              for every group I am in until I find a name with the permission level required.
     *              
     *      The groups file is a graph withe names a verticies and edges contain 
     *      GROUP_NAME MEMBER_NAME Grant|Revoke|Admin AMOUNT
     *
     */
    class block
    {
        public:
        block():version(0),time(0),salt(0){}
        /**
         *  The hash of the previous block.  
         *  0 if origin.
         */
        fl::sha1_hashcode    prev_block;
        uint16_t             version;

        /**
         *  Seconds from Jan 1, 2000 12:00:00AM
         *  Must be greater than prev_block->time by 300 seconds
         *  Current UTC time must be greater than time for the block to be valid
         */
        uint32_t             time; // seconds from Jan, 1, 2000

        /**
         *  The hash of the contents of the name database.
         */
        fl::sha1_hashcode    name_db_hash;

        /**
         *  The hash of the contents of the account database
         */
        fl::sha1_hashcode    account_db_hash;
        fl::sha1_hashcode    permission_db_hash;

        // the hash of the std::vector<dtdb::signed_transaction> included with this block
        fl::sha1_hashcode    transaction_hash;
        fl::sha1_hashcode    generator_name;
        uint64_t             salt;

        bool operator == ( const block& b )const
        {
            return prev_block == b.prev_block;
        }
    };


} // gpm

FL_SERIALIZE( dtdb::block,
    (prev_block)
    (version)
    (time)
    (name_db_hash)
    (account_db_hash)
    (permission_db_hash)
    (transaction_hash)
    (generator_name)
    (salt)
 )


#endif
