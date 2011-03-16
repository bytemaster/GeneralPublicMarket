#ifndef _TRX_FILE_HPP_
#define _TRX_FILE_HPP_
#include <gpm/exception.hpp>
#include <boost/filesystem.hpp>
#include <boost/enable_shared_from_this.hpp>


namespace gpm {


struct eof : public virtual gpm::exception
{
    const char*  what()const throw() { return "end of file"; }
    virtual void rethrow()const { BOOST_THROW_EXCEPTION(*this); }
};


/**
 *  @class abstract_file
 *  @brief Provides a generic interface for reading/writing to a file.
 *
 *  Inherits retainable because we do not want a file to disapear out
 *  from under someone.
 *
 */
class abstract_file : public boost::enable_shared_from_this<abstract_file>
{
    public:
        typedef boost::shared_ptr<abstract_file> ptr;
        virtual ~abstract_file(){};

        virtual uint64_t   read( char* data, uint64_t len ) = 0;
        virtual uint64_t   write( const char* data, uint64_t len ) = 0;
        virtual void       seek( uint64_t pos ) = 0;
        virtual uint64_t   size() = 0;
        virtual uint64_t   pos() = 0;
        virtual bool       eof()const = 0;

};


/**
 *  Wraps a FILE* and implements the abstract_file interface
 */
class file : public abstract_file
{
    public:
        typedef boost::shared_ptr<file> ptr;
        file( const boost::filesystem::path& p, const char* mode = "rb" );
        file();

        void open( const boost::filesystem::path& p, const char* mode = "rb" );
        void close();

        virtual uint64_t   read( char* data, uint64_t len );
        virtual uint64_t   write( const char* data, uint64_t len );
        virtual void       seek( uint64_t pos );
        virtual uint64_t   size();
        virtual uint64_t   pos();
        virtual bool       eof()const;

        void flush();

        ~file();

    private:
        FILE*                    m_file;
        boost::filesystem::path  m_path;
};


/**
 *  Caches writes to an abstract_file.  This class is designed to allow
 *  transactions to be chained together.  
 *
 *  Transactions can point to other transactions, but having two
 *  parallel transactions open at once can result in errors because
 *  they will not see eachothers changes and thus will stomp on them if
 *  two parallel transactions modify parts of the same page.
 */
class file_transaction : public abstract_file
{
    public: 
        typedef boost::shared_ptr<file_transaction> ptr;
        struct page 
        { 
            char data[1024];
            operator char*(){ return data; }
            operator const char*()const { return data; }
        };

        file_transaction( const abstract_file::ptr& base );
        ~file_transaction(){};


        virtual uint64_t   read( char* data, uint64_t len );
        virtual uint64_t   write( const char* data, uint64_t len );
        virtual void       seek( uint64_t pos );
        virtual uint64_t   size();
        virtual uint64_t   pos();
        virtual bool       eof()const;

        bool    commit();

        // be careful that the old source and new source contain the same data
        // this is needed to support deep copying of chained transactions
        void  change_source( const abstract_file::ptr& s ) { m_file = s; }
        const abstract_file::ptr& source()const { return m_file; }
        
    private:
        page* find_page( uint64_t p );
        page& dirty_page( uint64_t p );
        uint64_t                 m_pos;
        uint64_t                 m_size;
        std::map<uint64_t, page> m_dirty_pages;
        abstract_file::ptr       m_file;
};

} // namespace gpm

#endif
