#include <gpm/crypto/crypto.hpp>
#include <gpm/exception.hpp>
#include <boost/rpc/log/log.hpp>
#include <boost/rpc/protocol_buffer.hpp>
#include <string.h>
#include <openssl/dh.h>
#include <openssl/engine.h>
using namespace gpm;

int main( int argc, char** argv )
{
    DH* dh = DH_generate_parameters( 80, 5, NULL, NULL );
    DHparams_print_fp(stderr, dh );
    DH_generate_key(dh);
    DH* dh2 = DH_new(); 
    dh2->p = BN_dup(dh->p);
    dh2->g = BN_dup(dh->g);
    DH_generate_key(dh2);
    DHparams_print_fp(stderr, dh2 );
    
    slog( "dh keysize: %1%", DH_size(dh) );
    slog( "dh2 keysize: %1%", DH_size(dh2) );
    std::vector<unsigned char> key(DH_size(dh));
    std::vector<unsigned char> key2(DH_size(dh));
    DH_compute_key( &key.front(), dh2->pub_key, dh );
    DH_compute_key( &key2.front(), dh->pub_key, dh2 );


//    return 0;



    private_key<> priv;
    public_key<> pub;
    slog( "generating keys.." );
    generate_keys( pub, priv );

    slog( "creating test string.." );
    std::string hello( "Hello World" );
    for( uint32_t i = 0; i < 4; ++i ) 
        hello += hello;
    std::vector<char>  in( hello.begin(), hello.end() );
    in.reserve( 1024 );

    boost::rpc::sha1_hashcode digest = boost::rpc::hash_sha1(&in.front(),in.size());
    signature<> sig;

    slog( "starting signature test.." );
    priv.sign( digest, sig );
    if( pub.verify( digest, sig ) )
    {
        slog( "signatures match!" );
    }
    else
    {
        elog(  "signature mismatch" );
    }

    


    std::vector<char> eout;
    std::vector<char> dout;
    dout.reserve(1024);
    pub.encrypt( in, eout ); 
    slog( "esize: %1%", eout.size() );
    priv.decrypt( eout, dout ); 

    if( dout.size() == in.size() )
    {
        slog( "memcmp(in,dout) = %1%", memcmp( &dout.front(), &in.front(), in.size() ) );
    }
    else
    {
        elog(  "decrypted size %1% is different than input size %2%", dout.size(), in.size() );
    }
    return 0;
}
