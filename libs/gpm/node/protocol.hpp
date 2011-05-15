#include <gpm/block_chain/block.hpp>

namespace gpm {

    namespace proto {

        struct message
        {
            uint16_t                     id;
            std::vector<char>            data;

            message():id(0){}

            template<typename MsgType>
            message( const MsgType& m )
            :id(MsgType::id)
            {
                boost::rpc::raw::pack( data, m );
            }
            template<typename MsgType>
            operator MsgType ()const 
            { 
                if( id != MsgType::id ) 
                    THROW_GPM_EXCEPTION( "Invalid attempt to cast command to %1%", %boost::reflect::get_typeinfo<MsgType>::name() ); 
                MsgType m;
                boost::rpc::raw::unpack( data, m );
                return m;
            }
        };

        struct add_transaction
        {
            enum id_enum { id = 0x01 };
            signed_transaction trx;
        };
        struct get_full_block
        {
            enum id_enum { id = 0x02 };
            uint32_t index;
        };
        struct report_full_block
        {
            enum id_enum { id = 0x03 };
            report_full_block( const full_block_state& s = full_block_state())
            :state(s){}
            full_block_state  state;
        };
        struct get_head_block_index
        {
            enum id_enum { id = 0x04 };
        };
        struct report_head_block_index
        {
            enum id_enum { id = 0x05 };
            report_head_block_index( int32_t i = 0):index(i){}
            int32_t index;
        };
    } // namespace proto
 } // namespace gpm

 BOOST_REFLECT( gpm::proto::message,                 BOOST_PP_SEQ_NIL, (id)(data) )
 BOOST_REFLECT( gpm::proto::add_transaction,         BOOST_PP_SEQ_NIL, (trx) )
 BOOST_REFLECT( gpm::proto::get_full_block,          BOOST_PP_SEQ_NIL, (index) )
 BOOST_REFLECT( gpm::proto::report_full_block,       BOOST_PP_SEQ_NIL, (state) )
 BOOST_REFLECT( gpm::proto::get_head_block_index,    BOOST_PP_SEQ_NIL, BOOST_PP_SEQ_NIL )
 BOOST_REFLECT( gpm::proto::report_head_block_index, BOOST_PP_SEQ_NIL, (index) )



