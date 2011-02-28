SET( headers 
    crypto.hpp
    blowfish.hpp
    dh.hpp
    )
     
SET( sources
    blowfish.cpp
    crypto.cpp
    dh.cpp
   )

SET( libraries 
     crypto
     ${Boost_SYSTEM_LIBRARY} 
     ${Boost_THREAD_LIBRARY} 
   )

INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} /usr/include )

INCLUDE( SetupTargetMacros )

SETUP_LIBRARY( gpm_crypto   SOURCES   ${sources}
                            LIBRARIES ${libraries} 
                            AUTO_INSTALL_HEADERS 
                            LIBRARY_TYPE ${LIBRARY_BUILD_TYPE} )

ADD_EXECUTABLE( gpm_crypto_test crypto_test.cpp )
TARGET_LINK_LIBRARIES( gpm_crypto_test gpm_crypto ${libraries}  )

