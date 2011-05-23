set( node_sources rproxy.cpp proxy_connection.cpp control_connection.cpp main.cpp )

SET( libraries
    gpm_crypto
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_SIGNALS_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    db_cxx.a
    ${QT_LIBRARIES}
)

add_executable( rproxy ${node_sources} )
target_link_libraries( rproxy ${libraries} )


#add_executable( bbserver ${server_sources} )
#target_link_libraries( bbserver ${libraries} )

#add_executable( bbclient ${client_sources} )
#target_link_libraries( bbclient ${libraries} )

