#ifndef _GPM_CRYPTO_DH_HPP_
#define _GPM_CRYPTO_DH_HPP_
#include <boost/rpc/datastream/datastream.hpp>
#include <boost/reflect/reflect.hpp>

namespace gpm {

    struct diffie_hellman
    {
        diffie_hellman():valid(0){}
        bool generate_params( int s, uint8_t g );
        bool generate_pub_key();
        bool compute_shared_key( const std::vector<char>& pubk);
        bool validate();

        std::vector<char> p;
        std::vector<char> pub_key;
        std::vector<char> priv_key;
        std::vector<char> shared_key;
        bool              valid;
        uint8_t           g; 
    };

} // namespace gpm


BOOST_REFLECT( gpm::diffie_hellman, BOOST_PP_SEQ_NIL,
    (p)
    (g)
    (pub_key)
)

#endif
