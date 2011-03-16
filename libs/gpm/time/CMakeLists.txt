SET( headers 
    usclock.hpp
    )
     
SET( sources
    usclock.cpp
   )

SET( libraries 
     ${Boost_SYSTEM_LIBRARY} 
     ${Boost_THREAD_LIBRARY} 
     ${Boost_FILESYSTEM_LIBRARY} 
   )

INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} /usr/include )

INCLUDE( SetupTargetMacros )

SETUP_LIBRARY( gpm_time   SOURCES   ${sources}
                            LIBRARIES ${libraries} 
                            AUTO_INSTALL_HEADERS 
                            LIBRARY_TYPE ${LIBRARY_BUILD_TYPE} )

