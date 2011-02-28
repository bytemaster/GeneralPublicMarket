#ifndef __GPM_EXCEPTION_HPP_
#define __GPM_EXCEPTION_HPP_
#include <exception>
#include <sstream>
#include <boost/format.hpp>
#include <boost/exception/all.hpp>
#include <boost/exception/diagnostic_information.hpp>

namespace gpm
{
    typedef boost::error_info<struct gpm_msg_,std::string> gpm_msg;

    struct exception : public virtual boost::exception, public virtual std::exception
    {
        const char* what()const throw()               { return "gpm::exception";                       }
        virtual void       rethrow()const             { BOOST_THROW_EXCEPTION(*this);                  } 
        const std::string& message()const             { return *boost::get_error_info<gpm_msg>(*this); }
    };
}

#define THROW_GPM_EXCEPTION( fmt, ... ) \
        do { \
            std::stringstream ss; ss << boost::format(fmt) __VA_ARGS__;\
            BOOST_THROW_EXCEPTION( ::gpm::exception() << ::gpm::gpm_msg(ss.str()) ); \
        } while(0)

#endif
