#include "server.hpp"
#include <boost/rpc/json.hpp>

namespace gpm {
    void connection::start()
    {
        read_message();
        send_message( proto::get_full_block(m_node->get_head_block_index() + 1) );

        new_trx_con   = m_node->new_transaction.connect( boost::bind( &connection::send_transaction, this, _1 ) );
        new_block_con = m_node->new_block.connect( boost::bind( &connection::send_full_block, this, _1 ) );
    }
    void connection::handle_message( const gpm::proto::message& msg )
    {
        if( msg.id == proto::report_transaction::id )
        {
//            slog( "received transaction" );
            m_node->exec( boost::bind( &connection::add_transaction, this, msg) );
        }
        else if( msg.id == proto::get_full_block::id )
        {
//            slog( "received full block request" );
            proto::get_full_block gfb;
            gfb = msg;
            m_node->exec( boost::bind( &connection::get_full_block, this, gfb.index ) );
        }
        else if( msg.id == proto::get_head_block_index::id )
        {
//            slog( "received get_head_block_index request" );
            m_node->exec( boost::bind( &connection::get_head_block_index, this ) );
        }
        else if( msg.id == proto::report_full_block::id )
        {
//            slog( "received report full block" );
            m_node->exec( boost::bind( &connection::add_full_block, this, msg ) );
        }
        else
        {
            elog( "Unhandled message: %1%", boost::rpc::to_json(msg) );
        }
    }

    void connection::add_full_block(  const proto::report_full_block& rfb )
    {
        // TODO: temp disable new_block signal connection
        new_block_con.block();
        if( m_node->add_full_block( rfb.state ) > 0 && rfb.state.head_index > rfb.state.index )
            send_message( proto::get_full_block(m_node->get_head_block_index() + 1) );
        new_block_con.unblock();
    }

    void connection::add_transaction( const proto::report_transaction& at )
    {
        new_trx_con.block();
        m_node->add_transaction( at.trx );
        new_trx_con.unblock();
    }

    void connection::get_full_block(int32_t index)
    {
        try {
           send_message( proto::report_full_block( m_node->get_full_block( index ) ) );
        } 
        catch ( const boost::exception& e )
        {
            wlog( "%1%", boost::diagnostic_information(e) );
        }
    }
    // called via node->exec()
    void connection::get_head_block_index()
    {
        send_message( proto::report_head_block_index( m_node->get_head_block_index() ) );
    }

    void connection::send_message( const proto::message& msg )
    {
        std::vector<char>* data = new std::vector<char>();
        boost::rpc::raw::pack( *data, msg );
        boost::asio::async_write( sock, boost::asio::buffer( *data ),
                                  boost::bind( &connection::handle_write, this, 
                                               data, boost::asio::placeholders::error ) );
    }

    void connection::handle_write( std::vector<char>* data, const boost::system::error_code& err )
    {
        delete data;
        if( err )
        {
            closed();
            elog( "Error writing data..." );
        }
    }

    void connection::send_transaction( const signed_transaction& trx )
    {
        
       send_message( proto::report_transaction( trx ) );
    }
    void connection::send_full_block( const full_block_state& blk )
    {
       send_message( proto::report_full_block( blk ) );
    }

    void connection::read_message( int state, const boost::system::error_code& ec, size_t bytes_read )
    {
        //slog( "state: %1% read %2%", state, bytes_read );
        if( ec )
        {
            elog( "Error processing message: %1%", boost::system::system_error(ec).what() );
            closed();
            return;
        }
        switch( state )
        {
            case 0: // start by reading the id 
               boost::asio::async_read(  sock, boost::asio::buffer( (char*)&msg.id, sizeof(msg.id) ),
                                         boost::bind( &connection::read_message, this, state + 1, 
                                                      boost::asio::placeholders::error, 
                                                      boost::asio::placeholders::bytes_transferred ) );
               return;
            case 1: // then read the size
                boost::asio::async_read( sock, boost::asio::buffer( (char*)&msg_size, sizeof(msg_size) ),
                                         boost::bind( &connection::read_message, this, state + 1, 
                                                      boost::asio::placeholders::error, 
                                                      boost::asio::placeholders::bytes_transferred ) );

                return;
            case 2: // then read the data
                msg.data.resize(msg_size);
                boost::asio::async_read( sock, boost::asio::buffer( msg.data ),
                                         boost::bind( &connection::read_message, this, state + 1, 
                                                      boost::asio::placeholders::error, 
                                                      boost::asio::placeholders::bytes_transferred ) );

                return;
            case 3: // then process the message
                handle_message( msg );
                   
                // start reading the next message
                read_message();

        }
    }

}
