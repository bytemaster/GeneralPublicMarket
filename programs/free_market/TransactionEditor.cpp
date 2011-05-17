#include "TransactionEditor.hpp"
#include "CommandEditor.hpp"
#include "ui_transaction_edit.h"
#include <QCheckBox>
#include <boost/bind.hpp>

TransactionEditor::TransactionEditor( QWidget* parent )
:QWidget(parent)
{
   ui = new Ui_TransactionEditor();
   ui->setupUi(this);
   addCommand();
}

void TransactionEditor::save()
{
}
void TransactionEditor::submit()
{
}
void TransactionEditor::addCommand()
{
    CommandEditor* e = (CommandEditor*)sender();
    int f = -1;
    for( int i = 0; i < ui->command_layout->count(); ++i )
    {
        if( ui->command_layout->itemAt(i)->widget() == e )
        {
            f = i;
            break;
        }
    }
    CommandEditor* newe =  new CommandEditor(this);
    ui->command_layout->insertWidget(f+1, newe );
    connect( newe, SIGNAL( append() ), this, SLOT( addCommand() ) );
    newe->requiredSignaturesChanged.connect( boost::bind( &TransactionEditor::updateSignatures, this ) );
}

void TransactionEditor::updateSignatures()
{
    slog( "Update Signatures" );
    std::vector<std::string> req_sigs;
    std::vector<gpm::command> m_cmds(ui->command_layout->count() );
    for( uint32_t i = 0; i < ui->command_layout->count(); ++i )
    {
        AbstractCommandForm* acf = dynamic_cast<AbstractCommandForm*>(ui->command_layout->itemAt(i)->widget());
        if( acf )
        {
            std::vector<std::string> rs = acf->getRequiredSignatures();
            req_sigs.insert( req_sigs.end(),  rs.begin(), rs.end()  );
        }
        else
        {
            elog( "item not AbstractCommandForm" );
        }
    }
    for( uint32_t i = 0; i < ui->sig_layout->count(); ++i )
    {
        delete ui->sig_layout->itemAt(i)->widget();
    }
    std::sort(req_sigs.begin(), req_sigs.end() );
    req_sigs.erase( std::unique(req_sigs.begin(),req_sigs.end() ), req_sigs.end() );
    for( uint32_t i = 0; i < req_sigs.size(); ++i )
    {
        QCheckBox* cb = new QCheckBox( req_sigs[i].c_str(), this ); 
        ui->sig_layout->addWidget(cb);
        slog("Signatures %1%", req_sigs[i] );
    }
}
