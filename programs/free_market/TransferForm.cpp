#include "TransferForm.hpp"
#include "ui_transfer_form.h"
#include "keychain.hpp"

TransferForm::TransferForm( QWidget* w )
:QWidget(w)
{
    m_ui = new Ui_TransferForm();
    m_ui->setupUi(this);

    connect( m_ui->from, SIGNAL( currentIndexChanged( const QString& ) ),
             this, SLOT( currentFromChanged( const QString& ) ) );
    boost::shared_ptr<dtdb::node> n = get_node();
    std::vector<std::string>& names= get_known_names();
    for( uint32_t i = 0; i < names.size(); ++i )
    {
        m_ui->from->addItem( names[i].c_str() );
        m_ui->to->addItem( names[i].c_str() );
        
//        if( n->has_balance( names[i], names[i] ) )
//            m_ui->what->addItem(names[i].c_str() );
    }

}
void TransferForm::currentFromChanged( const QString& txt )
{
    m_ui->what->clear();
    fl::vector16<fl::pstring8> ct = get_node()->get_account_contents( txt.toStdString() );
    for( uint32_t i = 0; i < ct.size(); ++i )
    {
        uint64_t b = get_node()->get_balance(txt.toStdString(), ct[i] ); 
        QString txt(ct[i].c_str());
        txt += " (" + QString::number(b) + ")";
        m_ui->what->addItem( txt );
    }
    requiredSignaturesChanged.emit();
}
void TransferForm::createCommand( dtdb::command& cmd )const
{

}

std::vector<std::string> TransferForm::getErrors()const
{
    return std::vector<std::string>();
}

std::vector<std::string> TransferForm::getRequiredSignatures()const
{
    return std::vector<std::string>(1,m_ui->from->currentText().toStdString());
}
