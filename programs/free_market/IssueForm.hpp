#ifndef _ISSUE_FORM_HPP_
#define _ISSUE_FORM_HPP_
#include <QWidget>
#include "AbstractCommandForm.hpp"

class Ui_IssueForm;

class IssueForm : public QWidget, virtual public AbstractCommandForm
{
    Q_OBJECT
    public:
        IssueForm( QWidget* parent );

        void createCommand( dtdb::command& cmd )const;
        std::vector<std::string> getErrors()const;
        std::vector<std::string> getRequiredSignatures()const;

    public Q_SLOTS:
        void currentStockChanged( const QString& txt );

    private:
        Ui_IssueForm* m_ui;
};

#endif
