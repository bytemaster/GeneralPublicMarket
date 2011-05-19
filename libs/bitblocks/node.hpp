#ifndef _NODE_HPP_
#define _NODE_HPP_
#include "block.hpp"
#include "protocol.hpp"

#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>

using namespace boost::asio::ip;

typedef boost::function<void(const boost::system::error_code&)> closure;

class node;
class connection : public boost::enable_shared_from_this<connection>
{
    public:
        typedef boost::shared_ptr<connection> ptr;

        connection( boost::asio::io_service& _ios, node* n );

        void start() { read_message(); }
        void send( const proto::message& m, const closure& c = closure() );

        tcp::socket& socket();
        void request_block( const sha1_hashcode& hc, std::vector<char>& data, uint64_t start, uint64_t len, const closure& c );

    private:
        void read_message( int state = 0, const boost::system::error_code& ec = boost::system::error_code(),
                                             size_t bytes_read = 0);
        void handle_message( const proto::message& msg );

        // address to send refunds to on disconnect
        std::string       remote_btc_address;

        // address created just for this connection, 
        std::string       local_btc_address;
        uint64_t          total_recv_btc;  //  btc received at local_btc_address
        uint64_t          total_spent_btc; //  amount spent on downloads

        struct pending_request : public boost::enable_shared_from_this<pending_request>
        {
            typedef boost::shared_ptr<pending_request> ptr;
            pending_request( std::vector<char>& d, const closure& c )
            :data(d),close(c){}

            closure            close;
            std::vector<char>& data;
        };
        typedef std::map<uint64_t, boost::shared_ptr<pending_request> > preq_map;
        preq_map          m_requests;
        int64_t           next_req_id;
        
        proto::message    msg;
        uint32_t          msg_size;
        node*             m_node;
        tcp::socket       sock;
};

class node {
    
    public:
        node( boost::asio::io_service& i, const boost::filesystem::path& dir );
        ~node();
    
        typedef boost::function<void(const std::string&,const boost::system::error_code&)> upload_file_closure;

        void        upload_file( const std::string& payto, const boost::filesystem::path& file, const upload_file_closure& c );
        void        upload_block( const std::string& payto, const std::vector<char>& data, const closure& d = closure() );
        void        download_urn( const std::string& urn, const boost::filesystem::path&, const closure& d = closure() );
        void        download_block( const sha1_hashcode& hc, std::vector<char>& dest, const closure& c = closure() );

        std::string import_file( const boost::filesystem::path& file );

        void        export_file( const std::string& key_hash, const boost::filesystem::path& dir );

        void        store_block( const sha1_hashcode& hc, const std::vector<char>& d, const std::string& payto = "" );
        void        read_block( const sha1_hashcode& hc, std::vector<char>& d, uint64_t start = 0, uint64_t len = -1 );

        void listen( uint16_t port );

        connection::ptr connect_to( const std::string& host_port );

    private: 
        friend class connection;
        void start_accept();
        void closed( const connection::ptr& c ); 
        void handle_accept( const connection::ptr& con, const boost::system::error_code& err );

        typedef std::list<connection::ptr>  connection_list;
        connection_list                     m_connections;

        boost::asio::io_service&            m_ios;
        boost::shared_ptr<tcp::acceptor>    m_acceptor;
        block_db                            m_blocks;
};

#endif
