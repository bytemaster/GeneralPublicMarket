#ifndef __KEYCHAIN_HPP
#define __KEYCHAIN_HPP
#include <gpm/bdb/keyvalue_db.hpp>
#include <gpm/node/node.hpp>
#include <QSettings>
#include <QStringList>

namespace gpm {

struct keychain_record 
{
    keychain_record(){}
    keychain_record( const gpm::public_key_t& pu, const gpm::private_key_t& pr, const std::string& a = "" )
    :alias(a),pub(pu),priv(pr){}

    std::string       alias;
    gpm::public_key_t  pub; 
    gpm::private_key_t priv;

};

typedef keychain_record key_info;


struct keychain_address
{
    keychain_address( const std::string& a = std::string()):address(a){}
    keychain_address( const public_key_t& pk ):address( encode_address(pk) ){}

    bool is_valid()const { return validate_address_format( address ); }
    operator const std::string&()const { return address; }
    bool operator > ( const keychain_address& a )const { return this->address > a.address; }
    bool operator < ( const keychain_address& a )const { return this->address < a.address; }
    bool operator == ( const keychain_address& a )const { return this->address == a.address; }
    bool operator != ( const keychain_address& a )const { return this->address != a.address; }
    
    std::string address;
};


/**
 * The keychain provides access to a secure, encrypted
 * database of public private keys.
 *
 * Each key is assigned a name and an address.
 *
 *
 */
class keychain : public bdb::keyvalue_db<keychain_address,keychain_record>
{
    public:
        typedef bdb::keyvalue_db<keychain_address,keychain_record> base_class;
        keychain(){}

        static keychain& get_default() {  static keychain m_kc; return m_kc; }        

        bool set( const keychain_address& k, const keychain_record& v )
        {
            if( !k.is_valid() ) 
                return false;
            base_class::set(k,v);
            return true;
        }
        bool get( const keychain_address& k, keychain_record& v )
        {
            if( !k.is_valid() )
                return false;
            return base_class::get(k,v);
        }

        bool get( const keychain_address& k, private_key_t& v )
        {
            if( !k.is_valid() )
                return false;
            keychain_record r;
            bool rtn;
            if( rtn = get( k, r ) ) {
                v = r.priv; 
            }
            return rtn;
        }

    private:
};


} // gpm

BOOST_REFLECT( gpm::keychain_address, BOOST_PP_SEQ_NIL,
    (address)
)
BOOST_REFLECT( gpm::keychain_record, BOOST_PP_SEQ_NIL,
    (alias)(pub)(priv)
)




inline gpm::keychain&  get_keychain() 
{
    return gpm::keychain::get_default();
}

inline std::vector<std::string>& get_known_names()
{
    static std::vector<std::string> known_names;
    static bool init = true;
    if( init )
    {
        init = false;

        std::map<int,std::string> accounts;
        QSettings pref;
        pref.beginGroup( "Accounts" );
        QStringList acnts = pref.childGroups();
        for( uint32_t i = 0; i < acnts.size(); ++i )
            known_names.push_back(acnts.at(i).toStdString());
    }
    return known_names;

}

const boost::shared_ptr<gpm::node>& get_node();


#endif
