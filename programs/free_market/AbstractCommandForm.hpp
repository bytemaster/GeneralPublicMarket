#ifndef ABSTRACT_COMMAND_FORM_HPP
#define ABSTRACT_COMMAND_FORM_HPP
#include <fl/signals/signal.hpp>
#include <dtdb/command.hpp>


class AbstractCommandForm
{
    public:
        virtual void createCommand( dtdb::command& cmd )const = 0;
        virtual std::vector<std::string> getErrors()const = 0;
        virtual std::vector<std::string> getRequiredSignatures()const = 0;

        fl::signal<void()> requiredSignaturesChanged;
};


#endif
