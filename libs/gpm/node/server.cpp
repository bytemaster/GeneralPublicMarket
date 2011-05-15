#include "server.hpp"

namespace gpm {
    void connection::handle_message( const gpm::proto::message& msg )
    {
            if( msg.id == proto::add_transaction::id )
            {
                proto::add_transaction at(msg);
                // exec in the node thread... 
                m_node->exec( boost::bind( &node::add_transaction, m_node, at.trx ) );
            }
            else if( msg.id == proto::get_full_block::id )
            {
                proto::get_full_block gfb(msg);
                m_node->exec( boost::bind( &connection::get_full_block, this, gfb.index ) );
            }
            else if( msg.id == proto::get_head_block_index::id )
            {
                m_node->exec( boost::bind( &connection::get_head_block_index, this ) );
            }
            else if( msg.id == proto::report_full_block::id )
            {
                proto::report_full_block rfb;
                rfb = msg;

            }
    }



    void server::send_transaction( const signed_transaction& trx )
    {
    }
    void server::send_full_block( const full_block_state& blk )
    {
    }

    void connection::get_full_block(int32_t index)
    {
       send_message( proto::report_full_block( m_node->get_full_block( index ) ) );
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
            elog( "Error writing data..." );
        }
    }
}
