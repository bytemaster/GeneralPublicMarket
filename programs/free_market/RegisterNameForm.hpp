#ifndef REGISTER_NAME_FORM_HPP
#define REGISTER_NAME_FORM_HPP
#include <QWidget>
#include "AbstractCommandForm.hpp"

class Ui_RegisterNameForm;

class RegisterNameForm : public QWidget, virtual public AbstractCommandForm
{
    Q_OBJECT
    public:
        RegisterNameForm( QWidget* w, const QString& label = "with key" );

        void createCommand( gpm::command& cmd )const;
        std::vector<std::string> getErrors()const;
        std::vector<std::string> getRequiredSignatures()const;
    
    public Q_SLOTS:
        void newKey();

    private:
        Ui_RegisterNameForm* m_ui;
};

#endif
