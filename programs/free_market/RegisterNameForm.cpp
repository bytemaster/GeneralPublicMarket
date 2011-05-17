#include "RegisterNameForm.hpp"
#include "ui_register_name.h"
#include "keychain.hpp"
#include <gpm/block_chain/block.hpp>
#include <gpm/node/node.hpp>
const boost::shared_ptr<gpm::node>& get_node()
;
RegisterNameForm::RegisterNameForm( QWidget* parent, const QString& label )
:QWidget(parent)
{
    m_ui = new Ui_RegisterNameForm();
    m_ui->setupUi(this);
    m_ui->label->setText(label);

    connect( m_ui->toolButton, SIGNAL( clicked() ), this, SLOT( newKey() ) );


}

void RegisterNameForm::newKey()
{
    gpm::public_key_t pub;
    gpm::private_key_t priv;

    std::string gen = generate_address( pub, priv );

    get_keychain().set( encode_address(pub), gpm::key_info( pub, priv ) );
    get_keychain().sync();

    m_ui->key_name->insertItem( 0, gen.c_str() );
}

gpm::public_key_t get_public_key( const std::string& name )
{
    if( false /* is base64 encoded name */ )
    {

    }
    else
    {
        gpm::public_key_t pub;
        get_node()->get_key_for_name( name, pub );
        return pub;
    }
}

void RegisterNameForm::createCommand( gpm::command& cmd )const
{
    std::string name = m_ui->name->currentText().toStdString();
    cmd = gpm::cmd::register_name( name, get_public_key( m_ui->key_name->currentText().toStdString() ) );
}

std::vector<std::string> RegisterNameForm::getErrors()const
{
    std::vector<std::string> errs;
    std::string name = m_ui->name->currentText().toStdString();
    if( !get_node()->can_register( name ) ) 
    {
        errs.push_back(std::string( "Unable to register name '" + name + "'." ));
    }

    return errs;
}

std::vector<std::string> RegisterNameForm::getRequiredSignatures()const
{
    std::vector<std::string> sigs;
    std::string name = m_ui->name->currentText().toStdString();
    if( !get_node()->can_register( name ) ) 
    {
        sigs.push_back(name) ;
    }
    return sigs;
}
