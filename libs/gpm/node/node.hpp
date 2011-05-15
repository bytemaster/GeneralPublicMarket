#ifndef _GPM_NODE_HPP_
#define _GPM_NODE_HPP_
#include <boost/reflect/reflect.hpp>
#include <boost/rpc/datastream/sha1.hpp>
#include <boost/preprocessor/seq.hpp>
#include <boost/signal.hpp>
#include <gpm/block_chain/block.hpp>
#include <gpm/block_chain/transaction.hpp>

#include <QtConcurrentRun>
#include <QCoreApplication>
#include <QEvent>
#include <boost/bind.hpp>

namespace gpm {
  class node_private;

    class QFunctorEventBase : public QEvent
    {
        public:
            QFunctorEventBase()
            :QEvent( QEvent::User )
            {
            }
            virtual void exec() = 0;
    };

    template<typename Functor>
    class QFunctorEvent: public QFunctorEventBase
    {
        public:
            QFunctorEvent( const Functor& f ):m_f(f){}

            virtual void exec()
            {
                m_f();
            }
        private:
            Functor m_f;
    };

    class QFunctorHandler : public QObject
    {
        public:
            bool event( QEvent* e )
            {
                QFunctorEventBase* fe = dynamic_cast<QFunctorEventBase*>(e);
                if( fe )
                {
                    fe->exec();
                    return true;
                }
                return false;
            }
    };

    template<typename Functor>
    QFunctorEventBase* make_event( const Functor& f ) { return new QFunctorEvent<Functor>(f); }



  struct trx_log
  {
      trx_log():date(-1000000ll*60*60*24){}
      trx_log (
          const std::string& _from,
          const std::string& _to,
          const std::string& _type,
          uint64_t     _amnt,
          uint64_t     _to_bal,
          uint64_t     _from_bal,
          uint64_t     _date  )
      :
          from(_from),
          to(_to),
          type(_type),
          amnt(_amnt),
          to_bal(_to_bal),
          from_bal(_from_bal),
          date(_date) {}

      std::string from;
      std::string to;
      std::string type;
      uint64_t     amnt;
      uint64_t     to_bal;
      uint64_t     from_bal;
      uint64_t     date;
  };

  /**
   *  This is the interface that a node exposes to the network.  It allows other
   *  nodes to connect and query information.
   */
  class node
  {
    public:
      typedef boost::shared_ptr<node> ptr;
      node();
      ~node();

      void  open( const boost::filesystem::path& data_dir, bool create = false );
      void  configure_generation( const std::string& name, bool on = true );
      const std::string& generation_name()const;
      bool  is_generating()const;

      boost::signal<void(const std::string&, bool)> generation_state_changed;
      bool           get_key_for_name( const std::string& name, public_key_t& pk );
      const block&   head_block()const;

      void dump( uint32_t start = 0, uint32_t len = 100000 );


      template<typename Functor>
      void exec( Functor f )
      {
        QCoreApplication::instance()->postEvent( &main_thread, make_event( f ) );
      }
    
    // interface
      bool                     can_register( const std::string& name );
      full_block_state         get_full_block( uint32_t index );                                     
      int32_t                  get_head_block_index()const;                                        
                               
      signed_transaction       get_transaction( const boost::rpc::sha1_hashcode& trx );
      std::vector<trx_log>     get_transaction_log( const std::string& account, 
                                                    const std::string& type,
                                                    uint64_t start_date = 0, 
                                                    uint64_t end_date = -1);
                               
      block                    get_block_by_index( uint32_t index );
      block_state              get_block_state( const boost::rpc::sha1_hashcode& blk_state );
      public_key_t             get_public_key_t( const std::string& name );
      bool                     has_balance(  const std::string& account, const std::string& type );
      uint64_t                 get_balance( const std::string& account, const std::string& type );
      std::vector<std::string> get_account_contents( const std::string& account );

      std::vector<char>        get_state_chunk( uint32_t part, const boost::rpc::sha1_hashcode& hash );
                               
      uint64_t                                  get_hashrate()const; // hash/sec
                               
      int                      add_transaction( const signed_transaction& trx );
      int                      add_block( const block& blk );
      int                      add_full_block( const full_block_state& fbs );

      boost::signal<void(const std::string&)>                new_name;

      boost::signal<void(const signed_transaction&)>          new_transaction;
      boost::signal<void(const full_block_state& )>           new_block;

    private:
      friend class node_private;
      node_private* my;     
      QFunctorHandler main_thread;
  };

} // namespace gpm

BOOST_REFLECT( gpm::trx_log, BOOST_PP_SEQ_NIL,
   (from)(to)(type)(amnt)(to_bal)(from_bal)(date)
)

BOOST_REFLECT( gpm::node, BOOST_PP_SEQ_NIL,
    ( get_transaction )
    ( get_full_block )
    ( get_block_by_index )
    ( get_block_state )
    ( get_public_key_t )
    ( get_balance )
    ( has_balance )
    ( get_hashrate )
    ( get_account_contents )
    ( get_state_chunk )
    ( add_transaction )
    ( add_block )
    ( add_full_block )

//    ( new_transaction )
//    ( new_block )
)

#endif // NODE_HPP
