SET( headers 
    keyvalue_db.hpp
    )
     
SET( sources
   )

SET( libraries 
     db_cxx.a
     ${Boost_SYSTEM_LIBRARY} 
     ${Boost_THREAD_LIBRARY} 
   )

INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} /usr/include )

INCLUDE( SetupTargetMacros )


ADD_EXECUTABLE( _keyvalue_db_test keyvalue_db_test.cpp )
TARGET_LINK_LIBRARIES( _keyvalue_db_test ${libraries}  )
