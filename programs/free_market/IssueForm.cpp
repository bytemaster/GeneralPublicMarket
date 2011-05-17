#include "IssueForm.hpp"
#include "ui_issue_form.h"

IssueForm::IssueForm( QWidget* parent )
:QWidget(parent)
{
    m_ui = new Ui_IssueForm();
    m_ui->setupUi(this);

    connect( m_ui->stock_name, SIGNAL( currentIndexChanged( const QString& ) ),
             this, SLOT( currentStockChanged( const QString& ) ) );
}
void IssueForm::createCommand( gpm::command& cmd )const
{

}

void IssueForm::currentStockChanged( const QString& txt )
{
    requiredSignaturesChanged();
}
std::vector<std::string> IssueForm::getErrors()const
{
    return std::vector<std::string>();
}

std::vector<std::string> IssueForm::getRequiredSignatures()const
{
    return std::vector<std::string>(1,m_ui->stock_name->currentText().toStdString());
}
