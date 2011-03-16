#include "trx_file.hpp"
#include <boost/rpc/log/log.hpp>


namespace gpm {

file::file()
:m_file(NULL){}

/**
 *  
 */
file::file( const boost::filesystem::path& p, const char* mode)
:m_file(NULL)
{
    open(p,mode);
}

file::~file()
{
    if( m_file )
    {
        close();
    }
}

void file::close()
{
    if(m_file)
        fclose(m_file );
}
void file::flush()
{
    if( m_file ) fflush(m_file);
}

void file::open( const boost::filesystem::path& p, const char* mode )
{
    close();
    m_path = p;
    m_file = fopen( p.native_file_string().c_str(), mode );
    if( !m_file )
        THROW_GPM_EXCEPTION( "Error opening file %1%: %2%", %p  %strerror(errno));
}

uint64_t file::read( char* data, uint64_t len )
{
    //rout( status, "file read %3% pos %1%  len %2%", %pos()  %len %this );
    uint64_t r = fread( data, 1, len, m_file );
    if( r != len && ferror(m_file) )
        THROW_GPM_EXCEPTION( "Error reading file %1%", %m_path );
    return r;
}
uint64_t file::write( const char* data, uint64_t len )
{
    //rout( status, "file write: pos %1%  len %2%", %pos()  %len );
    size_t r = fwrite( data, 1, len, m_file );
    if( r != len && ferror(m_file) )
        THROW_GPM_EXCEPTION( "Error writing file %1%", %m_path );
    return r;
}

void file::seek( uint64_t pos )
{
    fseeko( m_file, pos, SEEK_SET );
}

uint64_t file::pos()
{
    return ftello(m_file);
}

uint64_t file::size()
{
    uint64_t cp = pos();
    fseeko( m_file, 0, SEEK_END );
    uint64_t s = pos();
    seek( cp );
    return s;
}

bool file::eof()const { return feof(m_file); }


file_transaction::file_transaction( const abstract_file::ptr& base )
:m_file(base),m_pos(0)
{
//    rout( status, "NEW TRX %1%", %this );
    if( m_file ) 
    {
        m_pos = m_file->pos();
        m_size = m_file->size();
    }
    else
    {
        elog( "Construct transaction form empty file" );
    }
}

file_transaction::page* file_transaction::find_page( uint64_t p )
{
    std::map<uint64_t, page>::iterator itr = m_dirty_pages.find(p);
    if( itr != m_dirty_pages.end() )
        return &itr->second;
    return NULL;
}

/**
 *  Creates a dirty page, copying from file, if the page does not
 *  already exist, else return the existing dirty page.
 */
file_transaction::page& file_transaction::dirty_page( uint64_t p )
{
    std::map<uint64_t, page>::iterator itr = m_dirty_pages.find(p);
    if( itr != m_dirty_pages.end() )
        return itr->second;
    page pg;
    page& np = m_dirty_pages.insert( std::make_pair( p, pg ) ).first->second;
    memset( np, 0, sizeof(np) );
    
    uint64_t old_pos = m_file->pos();
    m_file->seek( p * sizeof(page) );
    m_file->read( np, sizeof(page) );
    m_file->seek( old_pos );
    return np;
}


/**
 *  Check dirty pages first, else read page from file
 */
uint64_t file_transaction::read( char* data, uint64_t len )
{
//    rout( status, "pos %1%  len %2%", %m_pos  %len );
//    rout( status, "filetrx read %3% pos %1%  len %2%", %m_pos  %len %this );
    uint64_t start_page = m_pos / sizeof(page);
    uint32_t start_page_pos = m_pos % sizeof(page);

    char* pos  = data;
    const char* end  = data + len;
    while( pos != end && m_pos != m_size )
    {
        uint32_t page_pos  = m_pos % sizeof(page);
//        rout( status, "pos-data: %1%   page_pos %2%  len: %3%   sizeof(page)-page_pos: %4%", %(pos-data) %page_pos %len %(sizeof(page)-page_pos) );
        uint32_t read_size = 
            std::min( uint64_t(sizeof(page) - page_pos), 
                      uint64_t(len - (pos - data) ) );

        if( m_pos + read_size >= m_size ) 
        {
            if( m_pos >= m_size )
                return (pos - data);
//            rout( status, "m_size: %1%   m_pos: %2%", %m_size %m_pos );
            read_size = m_size - m_pos;
        }

        page* p = find_page( m_pos / sizeof(page) );
        if( p ) memcpy( pos, &((*p)[page_pos]), read_size );
        else
        {
            uint64_t fp = m_file->pos();
            m_file->seek( m_pos );
            uint64_t r = m_file->read( pos, read_size );
            m_file->seek( fp );
            if( r != read_size )
            {
                m_pos += r;
                return (pos - data) + r;
            }
        }
        m_pos += read_size;
        pos   += read_size;
    }
    return (pos - data);
}

uint64_t file_transaction::write( const char* data, uint64_t len )
{
//    rout( status, "filetrx write %3% pos %1%  len %2%", %m_pos  %len %this );
    const char* pos = data;
    const char* end = data + len;
    while( pos != end )
    {
        uint32_t page_pos  = m_pos % sizeof(page);
        uint32_t write_size = std::min( uint64_t(sizeof(page) - page_pos), uint64_t(len - (pos - data) ) );
        memcpy( &dirty_page(m_pos/sizeof(page))[page_pos], pos, write_size );
        pos   += write_size;
        m_pos += write_size;
        if( m_pos > m_size )
            m_size = m_pos;
    }
    return pos - data;
}

uint64_t file_transaction::pos() { return m_pos; }
uint64_t file_transaction::size() { return m_size; }
bool     file_transaction::eof()const { return m_pos >= m_size; }

void     file_transaction::seek( uint64_t p ) { m_pos = p; }

bool file_transaction::commit()
{
    std::map<uint64_t,page>::iterator itr = m_dirty_pages.begin();
    while( itr != m_dirty_pages.end() )
    {
        uint64_t cp = m_file->pos();
        m_file->seek( itr->first * sizeof(page) );

        if( (itr->first + 1) * sizeof(page) < m_size )
        {
            if( !m_file->write( itr->second, sizeof(page) ) ) return false;
        }
        else
        {
            if( !m_file->write( itr->second, m_size - (itr->first * sizeof(page)) ) )
                return false;
        }
        m_file->seek(cp);
        ++itr;
    }
    m_dirty_pages.clear();
    return true;
}



} // namespace gpm

