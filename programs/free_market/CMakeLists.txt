include( ${QT_USE_FILE} )

SET( moc_headers
    MainWindow.hpp
    AccountTableModel.hpp
    ContentTableModel.hpp
    TransactionEditor.hpp
    TrxTableModel.hpp
    CommandEditor.hpp
    TransferForm.hpp
    RegisterNameForm.hpp
    IssueForm.hpp
)

SET( headers 
    ${moc_headers}
)

SET( sources
    MainWindow.cpp
    AccountTableModel.cpp
    ContentTableModel.cpp
    CommandEditor.cpp
    TrxTableModel.cpp
    TransactionEditor.cpp
    TransferForm.cpp
    RegisterNameForm.cpp
    IssueForm.cpp
    main.cpp
)

SET( libraries
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_SIGNALS_LIBRARY}
    db_cxx.a
    gpm_node
    ${QT_LIBRARIES}
)

QT4_WRAP_CPP( sources ${moc_headers} )
QT4_WRAP_UI( sources main_window.ui transaction_edit.ui transfer_form.ui command_widget.ui register_name.ui issue_form.ui )

add_executable( free_market MACOSX_BUNDLE ${sources} )
target_link_libraries( free_market ${libraries} )
