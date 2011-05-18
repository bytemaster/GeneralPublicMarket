set( server_sources server.cpp )
set( client_sources client.cpp )

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

add_executable( bbserver ${server_sources} )
target_link_libraries( bbserver ${libraries} )

add_executable( bbclient ${client_sources} )
target_link_libraries( bbclient ${libraries} )
