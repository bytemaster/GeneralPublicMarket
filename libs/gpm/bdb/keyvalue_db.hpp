#ifndef _GPM_BDB_KEYVALUE_DB_HPP_
#define _GPM_BDB_KEYVALUE_DB_HPP_
#include <db_cxx.h>
#include <boost/rpc/raw.hpp>
#include <boost/filesystem.hpp>

namespace gpm { namespace bdb {

/**
 *  This class should be have the same as std::map except the back end
 *  is a database.
 */
template<typename Key, typename Value>
class keyvalue_db
{
    public:
        enum state { not_found = -1 };
        typedef boost::shared_ptr<keyvalue_db> ptr;

        keyvalue_db( )
        :m_db(NULL)
        {
        }
        int  count()const
        {
            int c = 0;
            Dbc*         cur;
            Dbt          key;
            Dbt          val;
            m_db->cursor( NULL, &cur, 0 );
            int rtn = cur->get( &key,&val, DB_NEXT );
            while( rtn != DB_NOTFOUND )
            {
                ++c;
                rtn = cur->get( &key,&val, DB_NEXT );
            }
            return c;
        }

        void open( const boost::filesystem::path& p, const std::string& password = "" )
        {
            m_db = new Db(/*env*/0,0);
            try {
                if( password.size() )
                {
                    m_db->set_encrypt( password.c_str(), 0 );
                }
                m_db->set_bt_compare( &keyvalue_db::compare );
                m_db->open( NULL, p.native_file_string().c_str(), "logical_file_name", DB_BTREE, DB_CREATE /*oflags*/, 0 );
            } 
            catch ( const DbException& e )
            {
                elog( "Caught DbException" );
                m_db->err(e.get_errno(), "Database open failed");
            }
            catch( const std::exception& e )
            {
                elog( "Caught std::exception %1%", boost::diagnostic_information( e )  );
            }

        }

        ~keyvalue_db()
        {
            if( m_db )
                delete m_db;
        }

        bool remove( const Key& k )
        {
            std::vector<char> kd;
            boost::rpc::raw::pack(kd,k);
            Dbt key( (void*)&kd.front(), kd.size() );
            int rtn = m_db->del( 0, &key, 0 );
            if( rtn == DB_NOTFOUND )
                 return false;
            return true;
        }
        static int compare(Db *db, const Dbt *key1, const Dbt *key2)
        {
            Key _k1;
            Key _k2;
            boost::rpc::raw::unpack( (const char*)key1->get_data(), key1->get_size(), _k1 );
            boost::rpc::raw::unpack( (const char*)key2->get_data(), key2->get_size(), _k2 );
            if( _k1 > _k2 ) return 1;
            if( _k1 == _k2 ) return 0;
            return -1;
        }

        void set( const Key& k, const Value& v )
        {
            std::vector<char> kd;
            std::vector<char> vd;
            boost::rpc::raw::pack(vd,v);
            boost::rpc::raw::pack(kd,k);

            Dbt val( &vd.front(), vd.size() );
            Dbt key( &kd.front(), kd.size() );
            m_db->put( 0, &key, &val, 0 );
        }

        struct iterator 
        {
            bool end()
            {
                return rtn == DB_NOTFOUND; 
            }
            iterator& operator++() 
            {
                Dbt          key;
                Dbt          val;
                rtn = cur->get( &key, &val, DB_NEXT );
                if( rtn != DB_NOTFOUND )
                {
                    if( key.get_size() )
                        boost::rpc::raw::unpack( (const char*)key.get_data(), key.get_size(), m_key );
                    if( val.get_size() )
                        boost::rpc::raw::unpack( (const char*)val.get_data(), val.get_size(), m_value );
                }
                return *this;
            }
            iterator& operator++(int) 
            {
                Dbt          key;
                Dbt          val;
                rtn = cur->get( &key, &val, DB_NEXT );
                if( rtn != DB_NOTFOUND )
                {
                    if( key.get_size() )
                        boost::rpc::raw::unpack( (const char*)key.get_data(), key.get_size(), m_key );
                    if( val.get_size() )
                        boost::rpc::raw::unpack( (const char*)val.get_data(), val.get_size(), m_value );
                }
                return *this;
            }


            iterator()
            :cur(NULL),self(NULL)
            {
            }

            iterator( const iterator& itr )
            :self(itr.self)
            {
                if( itr.cur )
                {
                    itr.cur->dup(&cur, DB_POSITION);
                }
                else if( self )
                    self->m_db->cursor( NULL, &cur, 0 );
                rtn   = itr.rtn;
            }

            iterator& operator = ( const iterator& i )
            {
                if( &i == this )
                    return *this;
                self = i.self;
                if( i.cur )
                {
                    if( cur ) 
                        cur->close();
                    i.cur->dup(&cur, DB_POSITION );
                }
                rtn     = i.rtn;
                m_key   = i.m_key;
                m_value = i.m_value;
                return *this;
            }

            ~iterator()
            {
                if( cur ) 
                    cur->close();
            }


            Key   key()const   { return m_key; }
            const Value& value()const { return m_value;   }
            void  set( const Value& v )
            {
                m_value = v;
                std::vector<char> kd;
                std::vector<char> vd;
                boost::rpc::raw::pack(vd,v);
                boost::rpc::raw::pack(kd,m_key);

                Dbt val( &vd.front(), vd.size() );
                Dbt key( &kd.front(), kd.size() );
                cur->put( &key, &val, 0 );
            }
            void remove()
            {
                cur->del(0);
                rtn = DB_NOTFOUND;
            }

            private:
                iterator( keyvalue_db* s ):self(s)
                {
                    s->m_db->cursor( NULL, &cur, 0 );
                }
                friend class keyvalue_db;

                Key          m_key;
                Value        m_value;


                int          rtn;
                Dbc*         cur;
                keyvalue_db* self;
        }; // iterator
        iterator search( const Key& k )
        {
            iterator itr(this);
            itr.m_key = k;

            std::vector<char> kd;
            boost::rpc::raw::pack(kd,k);
            Dbt key( &kd.front(), kd.size() );
            Dbt          val;
            itr.rtn = itr.cur->get( &key, &val, DB_SET_RANGE );
            if( itr.rtn != DB_NOTFOUND )
            {
                Dbt key;
                itr.rtn = itr.cur->get( &key, &val, DB_CURRENT );
                boost::rpc::raw::unpack((const char*)key.get_data(),key.get_size(), itr.m_key );
                boost::rpc::raw::unpack((const char*)val.get_data(),val.get_size(), itr.m_value );
            }
            return itr;
        }
        iterator find( const Key& k )
        {
            iterator itr(this);
            itr.m_key = k;

            std::vector<char> kd;
            boost::rpc::raw::pack(kd,k);
            Dbt key( &kd.front(), kd.size() );
            Dbt          val;
            itr.rtn = itr.cur->get( &key, &val, DB_SET );
            if( itr.rtn != DB_NOTFOUND )
            {
                boost::rpc::raw::unpack((const char*)val.get_data(),val.get_size(), itr.m_value );
            }
            return itr;
        }
        iterator begin()
        {
            iterator itr(this);
            Dbt key;
            Dbt val;
            itr.rtn = itr.cur->get( &key, &val, DB_NEXT );
            if( itr.rtn != DB_NOTFOUND )
            {
                boost::rpc::raw::unpack((const char*)key.get_data(),key.get_size(), itr.m_key );
                boost::rpc::raw::unpack((const char*)val.get_data(),val.get_size(), itr.m_value );
            }
            return itr;
        }

        bool get( const Key& k, Value& v )
        {
            iterator itr = find(k);
            if( itr.end() ) { return false; }
             v = itr.value();
            return true;
        }
        boost::optional<Value> get( const Key& k )
        {
            iterator itr = find(k);
            if( itr.end() ) { return boost::optional<Value>(); }
            return itr.value();
        }
        void sync()
        {
            m_db->sync(0);
        }


    private:
        Db* m_db;
};


} } // namespace gpm

#endif
