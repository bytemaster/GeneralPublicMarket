#ifndef _COMMAND_EDITOR_HPP_
#define _COMMAND_EDITOR_HPP_
#include <QWidget>
#include "AbstractCommandForm.hpp"


class Ui_CommandEditor;
class TransferForm;
class RegisterNameForm;
class IssueForm;

class CommandEditor : public QWidget, virtual public AbstractCommandForm
{
    Q_OBJECT
    public:
        CommandEditor( QWidget* parent );

        virtual void createCommand( gpm::command& cmd )const          { return m_cur->createCommand(cmd);      }
        virtual std::vector<std::string> getErrors()const              { return m_cur->getErrors();             }
        virtual std::vector<std::string> getRequiredSignatures()const  { return m_cur->getRequiredSignatures(); }

        boost::signal<void()> requiredSignaturesChanged;

    public Q_SLOTS:
        void commandChanged( int index );

    Q_SIGNALS:
        void append();

    private:
        Ui_CommandEditor*     m_ui;
        TransferForm*         m_tran_form;
        RegisterNameForm*     m_rn_form;
        RegisterNameForm*     m_tn_form;
        IssueForm*            m_issue_form;
        AbstractCommandForm*  m_cur;
};

#endif 
