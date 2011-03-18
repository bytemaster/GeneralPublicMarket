#include "MainWindow.hpp"
#include "ui_main_window.h"
#include "AccountTableModel.hpp"
#include "ContentTableModel.hpp"
#include "TrxTableModel.hpp"
#include <QSortFilterProxyModel>
#include <QInputDialog>
#include <QMessageBox>
#include "TransactionEditor.hpp"



MainWindow::MainWindow( )
{
   ui = new Ui_main_window();
   ui->setupUi(this);
   atm = new AccountTableModel(this);
   ctn = new ContentTableModel(this);
   trx = new TrxTableModel(this);

   QSortFilterProxyModel* acnt_proxy_model = new QSortFilterProxyModel(this);
   acnt_proxy_model->setSourceModel(atm);
   acnt_proxy_model->setDynamicSortFilter(true);

   QSortFilterProxyModel* content_proxy_model = new QSortFilterProxyModel(this);
   content_proxy_model->setSourceModel(ctn);
   content_proxy_model->setDynamicSortFilter(true);

   QSortFilterProxyModel* trx_proxy_model = new QSortFilterProxyModel(this);
   trx_proxy_model->setSourceModel(trx);
   trx_proxy_model->setDynamicSortFilter(true);


   ui->account_table->horizontalHeader()->setStretchLastSection(true);
   ui->account_table->setEditTriggers( QAbstractItemView::NoEditTriggers );
   ui->account_table->setSelectionMode( QAbstractItemView::SingleSelection );
   ui->account_table->setSortingEnabled( true );
   ui->account_table->verticalHeader()->hide();
   ui->account_table->verticalHeader()->hide();
   ui->account_table->setModel(acnt_proxy_model);
   if( !connect( ui->account_table->selectionModel(), SIGNAL( selectionChanged( const QItemSelection& , const QItemSelection& ) ),
            this, SLOT( accountSelectionChanged( const QItemSelection& , const QItemSelection& ) ) ) )
   {
    exit(1);
   }

   ui->content_table->horizontalHeader()->setStretchLastSection(true);
   ui->content_table->setEditTriggers( QAbstractItemView::NoEditTriggers );
   ui->content_table->setSelectionMode( QAbstractItemView::SingleSelection );
   ui->content_table->setSelectionBehavior( QAbstractItemView::SelectRows );
   ui->content_table->setSortingEnabled( true );
   ui->content_table->verticalHeader()->hide();
   ui->content_table->verticalHeader()->hide();
   ui->content_table->setModel(content_proxy_model);
   connect( ui->content_table->selectionModel(), SIGNAL( selectionChanged( const QItemSelection& , const QItemSelection& ) ),
            this, SLOT( contentSelectionChanged( const QItemSelection& , const QItemSelection& ) ) );

   ui->trx_table->horizontalHeader()->setStretchLastSection(true);
   ui->trx_table->setEditTriggers( QAbstractItemView::NoEditTriggers );
   ui->trx_table->setSelectionMode( QAbstractItemView::SingleSelection );
   ui->trx_table->setSortingEnabled( true );
   ui->trx_table->setSelectionBehavior( QAbstractItemView::SelectRows );
   ui->trx_table->verticalHeader()->hide();
   ui->trx_table->verticalHeader()->hide();
   ui->trx_table->setModel(trx_proxy_model);


   connect( ui->actionNew_Account, SIGNAL( triggered() ), this, SLOT( newAccount() ) );
   connect( ui->actionGenerate_Blocks, SIGNAL( triggered() ), this, SLOT( generate() ) );
   connect( ui->actionConfigure_Generation, SIGNAL( triggered() ), this, SLOT( configureGeneration() ) );
   connect( ui->actionTransfer, SIGNAL( triggered() ), this, SLOT( newTransaction() ) );
}
MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::newTransaction()
{
    TransactionEditor* tr = new TransactionEditor(NULL);
    tr->show();
}

void MainWindow::newAccount() 
{
    bool ok;
    QString text = QInputDialog::getText(this, tr("Enter a new account name: "),
               tr("Account Name:"), QLineEdit::Normal,
                 "", &ok);
    if (ok && !text.isEmpty())
    {
        dtdb::public_key old_public_key;
        std::string n(text.toStdString());
        if( !get_node()->get_key_for_name( n, old_public_key ) )
        {
            QMessageBox::warning( 0, QString("Unknown Name"), QString(("'"+n+"' has not been registered, would you like to register it?").c_str()) );
            return;
        }

        atm->addAccount( text );
    }

    /*
        dtdb::public_key old_public_key;
        std::string n(text.toStdString());
        if( !m_node->get_key_for_name( n, old_public_key ) )
        {
            dtdb::cmd::register_name reg;
            reg.name = n;

            dtdb::private_key priv;
            dtdb::generate_keys(reg.pub_key,priv);
            m_keyc->set( reg.pub_key, priv );
            m_keyc->sync();

            dtdb::signed_transaction strx;
            strx.trx.commands.resize( strx.trx.commands.size() + 1 );
            strx.trx.commands.back().id = dtdb::cmd::register_name::id;
            fl::pack( strx.trx.commands.back().data, reg );

            m_nkm->set( n,reg.pub_key );
            m_nkm->sync();

            m_node->add_transaction( strx );
            atm->m_names.push_back(n);

            std::sort(atm->m_names.begin(),atm->m_names.end());
            atm->m_names.erase( std::unique( atm->m_names.begin(), atm->m_names.end() ), atm->m_names.end() );

            atm->name_added(atm->m_names.size()-1);
        }
        else
        {
            QMessageBox::warning( 0, QString("Name already Registered"), QString(("'"+n+"' has already been registered.").c_str()) );
        }
    
    }
    */
}
void MainWindow::generate()
{
     get_node()->configure_generation( get_node()->generation_name(), ui->actionGenerate_Blocks->isChecked() );
}
void MainWindow::configureGeneration()
{
    bool ok;
    QString text = QInputDialog::getText(this, tr("Which account should receive generation fees?"),
               tr("Account Name:"), QLineEdit::Normal,
                 get_node()->generation_name().c_str(), &ok);
    if (ok && !text.isEmpty())
    {
        get_node()->configure_generation( text.toStdString(), ui->actionGenerate_Blocks->isChecked() );
    }
}

void MainWindow::contentSelectionChanged( const QItemSelection & selected, const QItemSelection & deselected )
{
    rout( warn, "content selection changed." );
    std::vector<dtdb::trx_log> trx_log;
    for( uint32_t i = 0; i < selected.indexes().size(); ++i )
    {
        //rout( status, "selected  %1% %2%", %selected.indexes()[i].row()  %m_node->get_name( selected.indexes()[i].row() ) );
        m_selected_type = ctn->getType( selected.indexes()[i].row() );
    }
    trx->setTrxLog( m_selected_acnt, get_node()->get_transaction_log( m_selected_acnt, m_selected_type ) );

}
void MainWindow::accountSelectionChanged( const QItemSelection & selected, const QItemSelection & deselected )
{
    rout( status, "" );
    std::vector<std::pair<QString,uint64_t> > names;
    for( uint32_t i = 0; i < selected.indexes().size(); ++i )
    {
        //rout( status, "selected  %1% %2%", %selected.indexes()[i].row()  %m_node->get_name( selected.indexes()[i].row() ) );
        fl::vector16<fl::pstring8> ct = get_node()->get_account_contents( atm->getName(selected.indexes()[i].row()) );
        rout( status, "%1%", %fl::to_json(ct) );
        for( uint32_t t = 0; t < ct.size();++t )
        {
            names.push_back( std::make_pair( QString( ct[t].c_str() ), 
                        get_node()->get_balance(atm->getName(selected.indexes()[i].row()), ct[t] ) ) ); 
        }
        m_selected_acnt = atm->getName(selected.indexes()[i].row());
       /*
        int acnt = selected.indexes()[i].row();
        std::map<uint32_t,uint64_t> bal = m_node->get_contents( acnt );
        std::map<uint32_t,uint64_t>::const_iterator itr = bal.begin();
        while( itr != bal.end() )
        {
            std::map<uint32_t,uint64_t>::iterator c = name_bal.find( itr->first );
            if( c == name_bal.end() )
                name_bal[itr->first] = itr->second;
            else
                c->second += itr->second;
            ++itr;
        }
        */
    }
    ctn->setData(names);
    ui->content_table->selectionModel()->select( ctn->index( 0,0 ), QItemSelectionModel::Select );
    ui->content_table->selectionModel()->select( ctn->index( 0,1 ), QItemSelectionModel::Select );
    ui->content_table->update();

}
