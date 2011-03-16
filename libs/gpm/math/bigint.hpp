#ifndef _GPM_BIGINT_HPP
#define _GPM_BIGINT_HPP
#include <openssl/bn.h>
#include <boost/rpc/raw.hpp>

namespace gpm
{
    class bigint
    {
        public:
            bigint( const char* bige, uint32_t l )
            {
                n = BN_bin2bn( (const unsigned char*)bige, l, NULL );
            }
            bigint( unsigned long i = 0 )
            :n(BN_new())
            {
                BN_set_word( n, i );
            }
            bigint( const bigint& c )
            {
                n = BN_dup( c.n );
            }
            ~bigint()
            {
                BN_free(n);
            }
            bool is_negative()const { return BN_is_negative(n); }
            int64_t to_int64()const { return BN_get_word(n); }

            int64_t log2()const { return BN_num_bits(n); }
            bool operator < ( const bigint& c )const
            {
                return BN_cmp( n, c.n ) < 0;
            }
            bool operator > ( const bigint& c )const
            {
                return BN_cmp( n, c.n ) > 0;
            }
            bool operator >= ( const bigint& c )const
            {
                return BN_cmp( n, c.n ) >= 0;
            }
            bool operator == ( const bigint& c )const
            {
                return BN_cmp( n, c.n ) == 0;
            }

            bigint operator + ( const bigint& a )const
            {
                bigint tmp(*this);
                BN_add( tmp.n, n, a.n );
                return tmp;
            }
            bigint operator * ( const bigint& a )const
            {
                BN_CTX* ctx = BN_CTX_new();
                bigint tmp(*this);
                BN_mul( tmp.n, n, a.n, ctx );
                BN_CTX_free(ctx);
                return tmp;
            }
            bigint operator / ( const bigint& a ) const 
            {
                BN_CTX* ctx = BN_CTX_new();
                bigint tmp(*this);
                BN_div( tmp.n, NULL, n, a.n, ctx );
                BN_CTX_free(ctx);
                return tmp;
            }
            bigint operator - ( const bigint& a )const
            {
                bigint tmp(*this);
                BN_sub( tmp.n, n, a.n );
                return tmp;
            }


            bigint& operator = ( const bigint& a )
            {
                if( &a == this ) 
                    return *this;
                BN_copy( n, a.n );
                return *this;
            }

            template<typename T>
            friend inline boost::rpc::datastream<T>& operator<<( boost::rpc::datastream<T>& ds, const gpm::bigint& bi )
            {
                std::vector<unsigned char> tmp(BN_num_bytes( bi.n) );
                if( tmp.size() )
                {
                    BN_bn2bin( bi.n, &tmp.front() );
                    ds.write( (const char*)&tmp.front(), tmp.size() );
                }
                return ds;
            }

            friend inline std::ostream& operator << ( std::ostream& os, const gpm::bigint& bi )
            {
                std::vector<char> h;
                boost::rpc::raw::pack( h, bi );
                for( uint32_t i = 0; i < h.size(); ++i )
                    os << std::hex << h[i] << ' ';
                return os;
            }

        private:
            BIGNUM* n;
            
    };
}

#endif
