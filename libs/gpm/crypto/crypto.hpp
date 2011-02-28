#ifndef GPM_CRYPTO_HPP_
#define GPM_CRYPTO_HPP_
#include <boost/rpc/datastream/sha1.hpp>
#include <boost/reflect/reflect.hpp>

/**
 *  Define common crypto methods and data types to abstract underlying implementation.  
 *
 *  Initial implementation will do nothing and 'assume' that everything is valid. 
 *
 */
namespace gpm {

    template<uint32_t KeySize=2048>
    struct signature
    {
        char data[KeySize/8];
    };

    bool verify_data( const char* key, uint32_t key_size, uint32_t pe, const boost::rpc::sha1_hashcode& hc, const char* sig ); 
    bool sign_data( const std::vector<char>& key, uint32_t key_size, uint32_t pe, const boost::rpc::sha1_hashcode& hc, char* sig ); 
    bool public_encrypt( const char* key, uint32_t key_size, uint32_t pe, const std::vector<char>& in, std::vector<char>& out );
    bool public_decrypt( const char* key, uint32_t key_size, uint32_t pe, const std::vector<char>& in, std::vector<char>& out );
    bool private_encrypt( const std::vector<char>& key, uint32_t key_size, uint32_t pe, const std::vector<char>& in, std::vector<char>& out );
    bool private_decrypt( const std::vector<char>& key, uint32_t key_size, uint32_t pe, const std::vector<char>& in, std::vector<char>& out );
    bool generate_keys( char* pubkey, std::vector<char>& privkey, uint32_t key_size, uint32_t pe );

    template<uint32_t KeySize = 2048, uint32_t PublicExponent = 65537>
    struct private_key;

    template<uint32_t KeySize = 2048, uint32_t PublicExponent = 65537>
    struct public_key
    {

        public_key()                       { memset( key, 0, sizeof(key) );      }
        public_key( const public_key& pk ) { memcpy( key, pk.key, sizeof(key) ); }

        bool verify( const boost::rpc::sha1_hashcode& digest, const signature<KeySize>& sig )
        {
            return verify_data( key, sizeof(key), PublicExponent, digest, sig.data );
        }
        bool encrypt( const std::vector<char>& in, std::vector<char>& out )
        {
            return public_encrypt( key, KeySize, PublicExponent, in, out );
        }
        bool decrypt( const std::vector<char>& in, std::vector<char>& out )
        {
            return public_decrypt( key, KeySize, PublicExponent, in, out );
        }
        
        public_key& operator = ( const public_key& pk )
        {
            memcpy( key, pk.key, sizeof(key) );
            return *this;
        }
        bool operator == ( const public_key& pk )const
        {
            return 0 == memcpy( key, pk.key, sizeof(key) );
        }
        bool operator != ( const public_key& pk )const
        {
            return 0 != memcpy( key, pk.key, sizeof(key) );
        }
        bool operator > ( const public_key& pk )const
        {
           return memcmp( key, pk.key, sizeof(key) ) > 0;
        }
        bool operator < ( const public_key& pk )const
        {
           return memcmp( key, pk.key, sizeof(key) ) < 0;
        }

        private:
            template<uint32_t KS, uint32_t PE>
            friend void generate_keys( public_key<KS,PE>& pub, private_key<KS,PE>& priv );

            char key[KeySize/8];
    };


    template<uint32_t KeySize, uint32_t PublicExponent>
    struct private_key
    {
        bool encrypt( const std::vector<char>& in, std::vector<char>& out )
        {
            return private_encrypt( key, KeySize, PublicExponent, in, out );
        }
        bool decrypt( const std::vector<char>& in, std::vector<char>& out )
        {
            return private_decrypt( key, KeySize, PublicExponent, in, out );
        }
        bool sign( const boost::rpc::sha1_hashcode& digest, signature<KeySize>& sig )
        {
            return sign_data( key, KeySize, PublicExponent, digest, sig.data );
        }
        private:
            template<uint32_t KS, uint32_t PE>
            friend void generate_keys( public_key<KS,PE>& pub, private_key<KS,PE>& priv );
            std::vector<char> key;
    };

    template<uint32_t KeySize, uint32_t PublicExponent>
    void generate_keys( public_key<KeySize,PublicExponent>& pub, private_key<KeySize,PublicExponent>& priv )
    {
        generate_keys( pub.key, priv.key, KeySize, PublicExponent );
    }
    template<uint32_t KeySize>
    inline std::ostream& operator<< ( std::ostream& os, const signature<KeySize>& s )
    {
        for( uint32_t i = 0; i < KeySize; ++i )
            os << std::hex << int(s.data[i]) << ' ';
        return os;
    }

    typedef public_key<> public_key_t;
    typedef private_key<> private_key_t;
} // namespace gpm

/**
 *  Public keys are stored as a single ANET_RSA_KEY_BYTES byte number
 */
template<typename T,uint32_t KS, uint32_t PE>
boost::rpc::datastream<T>& operator<<( boost::rpc::datastream<T>& ds, const gpm::public_key<KS,PE>& pk )
{
    ds.write(pk.key, KS/8 );
    return ds;
}
template<typename T,uint32_t KS, uint32_t PE>
boost::rpc::datastream<T>& operator>>( boost::rpc::datastream<T>& ds, gpm::public_key<KS,PE>& pk )
{
    ds.read( pk.key, KS/8 );
    return ds;
}
template<typename T,uint32_t KS>
boost::rpc::datastream<T>& operator<<( boost::rpc::datastream<T>& ds, const gpm::signature<KS>& sig )
{
    ds.write(sig.data, KS/8 );
    return ds;
}
template<typename T,uint32_t KS>
boost::rpc::datastream<T>& operator>>( boost::rpc::datastream<T>& ds, gpm::signature<KS>& sig )
{
    ds.read(sig.data, KS/8 );
    return ds;
}
template<typename T,uint32_t KS, uint32_t PE>
boost::rpc::datastream<T>& operator<<( boost::rpc::datastream<T>& ds, const gpm::private_key<KS,PE>& pk )
{
    return ds << pk.key;
}
template<typename T,uint32_t KS, uint32_t PE>
boost::rpc::datastream<T>& operator>>( boost::rpc::datastream<T>& ds, gpm::private_key<KS,PE>& pk )
{
    return ds >> pk.key;
}

BOOST_REFLECT_TYPEINFO( gpm::public_key_t )
BOOST_REFLECT_TYPEINFO( gpm::private_key_t )

#endif
