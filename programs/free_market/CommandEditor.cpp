#include "CommandEditor.hpp"
#include "TransferForm.hpp"
#include "RegisterNameForm.hpp"
#include "IssueForm.hpp"
#include "ui_command_widget.h"


CommandEditor::CommandEditor( QWidget* parent )
:QWidget(parent)
{
   m_ui = new Ui_CommandEditor();
   m_ui->setupUi(this);

   connect( m_ui->command_selector, SIGNAL( currentIndexChanged( int ) ),
            this, SLOT( commandChanged(int ) ) );

    m_rn_form   = new RegisterNameForm( m_ui->command, "with key" ); 
    m_tn_form   = new RegisterNameForm( m_ui->command, "to key" ); 
    m_tran_form = new TransferForm( m_ui->command );
    m_issue_form = new IssueForm( m_ui->command );

    QSizePolicy sizePolicy2(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    sizePolicy2.setHorizontalStretch(0);
    sizePolicy2.setVerticalStretch(0);
    sizePolicy2.setHeightForWidth(m_tran_form->sizePolicy().hasHeightForWidth());
    m_tran_form->setSizePolicy(sizePolicy2);
    m_tran_form->setMinimumSize(QSize(600, 0));

    m_ui->command_layout->addWidget(m_tran_form);
    m_ui->command_layout->addWidget(m_rn_form);
    m_ui->command_layout->addWidget(m_tn_form);
    m_ui->command_layout->addWidget(m_issue_form);
    m_rn_form->hide();
    m_tn_form->hide();
    m_issue_form->hide();

    connect( m_ui->add, SIGNAL( clicked() ), this, SIGNAL( append() ) );
    connect( m_ui->remove, SIGNAL( clicked() ), this, SLOT( deleteLater() ) );

    m_cur = dynamic_cast<AbstractCommandForm*>(m_tran_form);
    m_tran_form->requiredSignaturesChanged.connect(requiredSignaturesChanged);
    m_rn_form->requiredSignaturesChanged.connect(requiredSignaturesChanged);
    m_tn_form->requiredSignaturesChanged.connect(requiredSignaturesChanged);
    m_issue_form->requiredSignaturesChanged.connect(requiredSignaturesChanged);
}

void CommandEditor::commandChanged( int index )
{
    if( m_ui->command_selector->currentText() == "Transfer Stock" )
    {
        m_tran_form->show();
        m_cur = dynamic_cast<AbstractCommandForm*>(m_tran_form);
        m_rn_form->hide();
        m_tn_form->hide();
        m_issue_form->hide();
    }
    else if( m_ui->command_selector->currentText() == "Register Name" )
    {
        m_rn_form->show();
        m_cur = dynamic_cast<AbstractCommandForm*>(m_rn_form);
        m_tran_form->hide();
        m_tn_form->hide();
        m_issue_form->hide();
    }
    else if( m_ui->command_selector->currentText() == "Transfer Name" )
    {
        m_tn_form->show();
        m_tran_form->hide();
        m_cur = dynamic_cast<AbstractCommandForm*>(m_tn_form);
        m_rn_form->hide();
        m_issue_form->hide();
    }
    else if( m_ui->command_selector->currentText() == "Issue Stock" )
    {
        m_tn_form->hide();
        m_tran_form->hide();
        m_rn_form->hide();
        m_issue_form->show();
        m_cur = dynamic_cast<AbstractCommandForm*>(m_issue_form);
    }
    else
    {
        m_issue_form->hide();
        m_rn_form->hide();
        m_tn_form->hide();
        m_tran_form->hide();
        m_cur = NULL;
    }
}
