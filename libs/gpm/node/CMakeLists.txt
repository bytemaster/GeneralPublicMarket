SET( headers 
    node.hpp
    )
     
SET( sources
    node.cpp
   )

SET( libraries 
     gpm_crypto
     gpm_time
     gpm_block_chain
     gpm_state_database
     db_cxx.a
     ${Boost_SYSTEM_LIBRARY} 
     ${Boost_THREAD_LIBRARY} 
     ${Boost_FILESYSTEM_LIBRARY} 
   )

INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} /usr/include )

INCLUDE( SetupTargetMacros )

SETUP_LIBRARY( gpm_node  SOURCES   ${sources}
                                   LIBRARIES ${libraries} 
                                   AUTO_INSTALL_HEADERS 
                                   LIBRARY_TYPE ${LIBRARY_BUILD_TYPE} )

#add_executable( gpm_state_database_test state_database_test.cpp )
#target_link_libraries( gpm_state_database_test gpm_state_database ${libraries} )
