#ifndef _STATE_LOG_HPP_
#define _STATE_LOG_HPP_
#include <fl/retainable/retainable.hpp>
#include <fl/datastream/datastream.hpp>
#include <dtdb/transaction.hpp>
#include <dtdb/block.hpp>
#include <dtdb/keyvalue_db.hpp>

namespace dtdb {
    struct account_key
    {
        account_key( uint64_t n = 0, uint64_t t = 0 )
        :account_name(n),type_name(t){}

        uint64_t account_name;
        uint64_t type_name;

        bool operator < ( const account_key& c )const
        {
            if( account_name < c.account_name )
                return true;
            return account_name == c.account_name && type_name < c.type_name;
        }
        bool operator > ( const account_key& c )const
        {
            if( account_name > c.account_name )
                return true;
            return account_name == c.account_name && type_name > c.type_name;
        }
        bool operator == ( const account_key& c )const
        {
            return  account_name == c.account_name && type_name == c.type_name;
        }
    };

    struct state_record
    {
        uint16_t          id;
        std::vector<char> data;
        uint64_t          previous;

        state_record():id(0),previous(0){}

        template<typename MsgType>
        state_record( const MsgType& m ):id(MsgType::id)
        {
            fl::pack( data, m );
        }

        template<typename MsgType>
        operator MsgType ()const 
        { 
            if( id != MsgType::id ) 
                THROW_TEXCEPTION( fl::exception(), "Invalid attempt to cast record to %1%", %fl::get_typename<MsgType>::str() ); 
            MsgType m;
            fl::unpack( data, m );
            return m;
        }
    };

    /**
     *  Maps the name to the last index that modified its public key
     */
    typedef std::map<std::string,uint64_t> name_index;

    /**
     * Maps the account / stock combo to the index of the last record that modified
     * them.
     */
    typedef std::map<account_key, uint64_t >  account_index;

    /**
     *   The purpose of the state log is to provide a complete history of all changes and
     *   to make it effecient to browse the transaction history.  
     *
     *   Requirements:
     *      1) Each entry must contain enough information to 'undo' any effects that
     *         it had on the state of various caching systems.
     * 
     *      2) Ability to branch the log and apply 'parallel' states without 
     *         disturbing the main.
     *
     *      3) The ability to provide callbacks anytime action occurs on 
     *         a monitored account.
     *
     *
     *   When generating the block chain there numerous states that all build
     *   off of a long history.  At any time we want to be able to query the
     *   database and revert to earlier states without having to start from
     *   the beginning state.  
     *
     *   B1 -> B2 -> Head -> Gen -> Testing New Transactions
     *            \_ Replace Head
     *
     *   The abstract state database defines the queries that may be performed
     *   upon the state.
     */
    class abstract_state_database : public fl::retainable
    {
        public:
            typedef fl::retainable_ptr<abstract_state_database> ptr;

            /**
             *  Enables iteration over changes to account balance
             */
            struct transfer 
            {
                transfer():m_index(-1){}
                transfer( const abstract_state_database::ptr& p, uint64_t index = -1 );

                /// goes to the previous transfer on account and returns false if there is no prior transfers
                bool                 previous( const std::string& account );

                bool                 end()const          { return m_index == -1; }
                std::string          from()const         { return m_from; }
                std::string          to()const           { return m_to;   }
                std::string          type()const         { return m_type; }
                uint64_t             amount()const       { return m_amount; }
                uint64_t             to_balance()const   { return m_to_bal; }
                uint64_t             from_balance()const { return m_from_bal; }
                uint64_t             time()const         { return m_time;     }

                private:
                    bool update();
                    friend class abstract_state_database;
                    friend class state_database_transaction;
                    uint64_t                     m_index;

                    std::string                  m_from;
                    std::string                  m_to;
                    std::string                  m_type;
                    uint64_t                     m_prev_from;
                    uint64_t                     m_prev_to;
                    uint64_t                     m_amount;
                    uint64_t                     m_from_bal;
                    uint64_t                     m_to_bal;
                    uint64_t                     m_time;
                    uint64_t                     m_trx_idx;
                    abstract_state_database::ptr asd;
            };

            virtual std::vector<std::string>  get_account_contents( const std::string& acnt ) = 0;
            virtual std::vector<uint64_t>     get_account_contents_idx( uint64_t acnt ) = 0;
            virtual uint64_t                  name_count() = 0;
            virtual std::vector<std::string>  query_names( const std::string& start, const std::string& end, uint32_t limit = -1 ) = 0;
            
            virtual bool     get_public_key( const std::string& name, public_key& pk       )                             = 0;
            virtual bool     set_public_key( const std::string& name, const public_key& pk )                             = 0;
            virtual std::string get_name_for_index(uint64_t idx)= 0;

            virtual uint64_t  get_name_index( const std::string& nidx )                                                   = 0;
            virtual transfer  get_last_transfer( const std::string& account, const std::string& type )                    = 0;

            virtual uint64_t get_balance( const std::string& account, const std::string& type, bool* is_null = 0, uint64_t* loc = 0 )   = 0;
            virtual void     transfer_balance( const std::string& from, const std::string& to, const std::string& type, uint64_t amnt, uint64_t trx_pos=-1 ) = 0;
            virtual void     issue( const std::string& type, uint64_t trx_pos = -1 ) = 0;

            virtual bool     apply( const signed_transaction& trx, const std::string& gen_name ) = 0;
            virtual bool     apply( const block& b, const std::string& gen, const std::vector<signed_transaction>& trxs, 
                                                                                                const fl::sha1_hashcode& check ) = 0;

            virtual fl::sha1_hashcode calculate_state_hash() = 0;
            virtual bool     append_record( const state_record& r ) = 0;
            virtual bool     append( std::vector<char>& d, const account_index& idx, const name_index& nidx ) = 0;

            virtual uint64_t get_record( uint64_t loc, state_record& r ) = 0;
            virtual uint64_t read( uint64_t pos, char* buf, uint64_t len ) = 0;

            virtual uint64_t  size()const = 0;
            virtual uint64_t  start()const = 0;


            virtual uint64_t      get_last_name_edit_index( const std::string& nidx ) = 0;
            virtual uint64_t      get_last_transfer_index( const account_key& a ) = 0;
    };


    /**
     *  This class implements the full interface for the state_database, but keeps all writes local
     *  until commit() is called.
     */
    class state_database_transaction : public abstract_state_database
    {
        public:
            typedef fl::retainable_ptr<state_database_transaction> ptr;
            state_database_transaction( const abstract_state_database::ptr& new_base = abstract_state_database::ptr() );

            void     dump(int max = 1000);
            void     dump(const state_record& r );

            // returns the number of names in the database
            virtual uint64_t                          name_count();

            // returns names between start and end in order limited to the first limit names after start. 
            virtual std::vector<std::string>  query_names( const std::string& start, const std::string& end, uint32_t limit = -1 );


            virtual std::vector<std::string>  get_account_contents( const std::string& acnt );
            virtual std::vector<uint64_t>     get_account_contents_idx( uint64_t acnt );
            block    find_last_block();

            bool     get_public_key( const std::string& name, public_key& pk       );
            bool     set_public_key( const std::string& name, const public_key& pk );
            
            std::string get_name_for_index(uint64_t idx);
            uint64_t    get_name_index( const std::string& nidx );
            transfer    get_last_transfer( const std::string& account, const std::string& type );
            
            uint64_t get_balance( const std::string& account, const std::string& type, bool* is_null = 0, uint64_t* loc = 0 );
            void     transfer_balance( const std::string& from, const std::string& to, const std::string& type, uint64_t amnt , uint64_t trx_pos = -1);
            void     issue( const std::string& type , uint64_t trx_pos = -1);
            
            bool     apply( const signed_transaction& trx, const std::string& gen_name );
            bool     apply( const block& b, const std::string& gen, const std::vector<signed_transaction>& trxs, 
                                                                                   const fl::sha1_hashcode& check );
            
            fl::sha1_hashcode calculate_state_hash();

            uint64_t         read( uint64_t pos, char* buf, uint64_t len );
            uint64_t         get_record( uint64_t loc, state_record& r );
            bool             append_record( const state_record& r );
            bool             append( std::vector<char>& d, const account_index& idx, const name_index& nidx );

            uint64_t  size()const;
            uint64_t  start()const;

            // writes local changes to base.
            virtual  bool commit();
            virtual  void abort();

            virtual void rebase( const abstract_state_database::ptr& new_base ) { base = new_base; }

            virtual uint64_t      get_last_name_edit_index( const std::string& nidx );
            virtual uint64_t      get_last_transfer_index( const account_key& a );

        protected:
            abstract_state_database::ptr      base;
            std::vector<char>                 local_changes;
            account_index                     last_transfer_map;
            name_index                        last_name_edit_map;
    };


    /**
     *  Identical to the state_databaase_transaction, except that commit writes
     *  the changes to the file.
     */
    class state_database : public state_database_transaction
    {
        public:
            typedef fl::retainable_ptr<state_database> ptr;
            state_database();
            
            bool open( const boost::filesystem::path& file );
            virtual uint64_t                  name_count() ;
            virtual std::vector<std::string>  query_names( const std::string& start, const std::string& end, uint32_t limit = -1 );
        
            uint64_t read( uint64_t pos, char* buf, uint64_t len );
            uint64_t get_record( uint64_t loc, state_record& r );

            uint64_t      get_last_name_edit_index( const std::string& nidx );
            uint64_t      get_last_transfer_index( const account_key& a );

            virtual std::vector<std::string>  get_account_contents( const std::string& acnt );
            virtual std::vector<uint64_t>     get_account_contents_idx( uint64_t acnt );
            bool      commit();
            uint64_t  size()const;
            uint64_t  start()const;
            
            void  update_index();

        private:
            void rebase( abstract_state_database::ptr& new_base ){};

            dtdb::file::ptr                      m_file;
            keyvalue_db<account_key,uint64_t>    m_transfer_db;
            keyvalue_db<std::string,uint64_t>    m_name_db;
    };
    struct define_name
    {
        enum id_enum{ id = 0x01 };
        define_name( const fl::pstring8& n = fl::pstring8(), const public_key& k = public_key())
        :name(n),key(k){}
        fl::pstring8 name;
        public_key   key;
    };
    struct update_name
    {
        enum id_enum{ id = 0x02 };
        update_name( uint64_t nidx = 0, const public_key& pk = public_key(), uint64_t lui = 0 )
        :name_idx(nidx),key(pk),last_update_idx(lui){}

        uint64_t     name_idx;
        public_key   key;
        uint64_t     last_update_idx;
    };
    struct transfer_log
    {
        enum id_enum{ id = 0x03 };
        transfer_log():last_from_trx(-1),last_to_trx(-1),trx_idx(-1){}
        uint64_t amount;
        uint64_t type_name;
        uint64_t from_name;
        uint64_t to_name;
        uint64_t new_from_bal;
        uint64_t new_to_bal;
        uint64_t last_from_trx;
        uint64_t last_to_trx;
        uint64_t trx_idx;
    };
    struct start_trx
    {
        enum id_enum{ id = 0x04 };
        start_trx( const fl::sha1_hashcode& hc = fl::sha1_hashcode(), uint64_t _utc_time = -1000000 )
        :trx_hash(hc),utc_time(_utc_time){}
        fl::sha1_hashcode trx_hash;
        uint64_t          utc_time;
    };
    struct end_block
    {
        enum id_enum{ id = 0x05 };
        end_block( const block& b, const std::string& g )
        :blk(b),generator(g){}
        end_block():block_num(-1){}
        block             blk;
        fl::pstring8      generator;
        uint32_t          block_num;
    };

} // namesapce dtdb

FL_SERIALIZE( dtdb::state_record,
    (id)
    (data)
    (previous)
)
FL_SERIALIZE( dtdb::define_name,
    (name)
    (key)
)
FL_SERIALIZE( dtdb::update_name,
    (name_idx)
    (key)
    (last_update_idx)
)
FL_SERIALIZE( dtdb::transfer_log,
    (amount)
    (type_name)
    (from_name)
    (to_name)
    (new_from_bal)
    (new_to_bal)
    (last_from_trx)
    (last_to_trx)
    (trx_idx)
)
FL_SERIALIZE( dtdb::start_trx,
    (trx_hash)(utc_time)
)
FL_SERIALIZE( dtdb::end_block,
    (blk)
    (generator)
)
FL_SERIALIZE( dtdb::account_key,
    (account_name)
    (type_name)
)

#endif
