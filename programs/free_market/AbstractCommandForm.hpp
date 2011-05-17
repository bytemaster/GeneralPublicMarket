#ifndef ABSTRACT_COMMAND_FORM_HPP
#define ABSTRACT_COMMAND_FORM_HPP
#include <boost/signals.hpp>
#include <gpm/block_chain/command.hpp>


class AbstractCommandForm
{
    public:
        virtual void createCommand( gpm::command& cmd )const = 0;
        virtual std::vector<std::string> getErrors()const = 0;
        virtual std::vector<std::string> getRequiredSignatures()const = 0;

        boost::signal<void()> requiredSignaturesChanged;
};


#endif
