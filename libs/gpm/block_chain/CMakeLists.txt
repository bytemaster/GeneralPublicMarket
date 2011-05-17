SET( headers 
    block.hpp
    command.hpp
    transaction.hpp
    )
     
SET( sources
    block.cpp
   )

SET( libraries 
     gpm_crypto
     gpm_time
     ${Boost_SYSTEM_LIBRARY} 
     ${Boost_THREAD_LIBRARY} 
     ${Boost_FILESYSTEM_LIBRARY} 
   )

INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} /usr/include )

INCLUDE( SetupTargetMacros )

SETUP_LIBRARY( gpm_block_chain   SOURCES   ${sources}
                            LIBRARIES ${libraries} 
                            AUTO_INSTALL_HEADERS 
                            LIBRARY_TYPE ${LIBRARY_BUILD_TYPE} )

add_executable( gpm_block_chain_test block_chain_test.cpp )
target_link_libraries( gpm_block_chain_test gpm_block_chain ${libraries} )
