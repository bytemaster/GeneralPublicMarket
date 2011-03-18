#include "AccountTableModel.hpp"
#include <boost/foreach.hpp>
#include <QSettings>
#include <QStringList>
#include "keychain.hpp"

AccountTableModel::AccountTableModel( QObject* p )
:QAbstractTableModel(p)
{
/*
   m_names.clear();
   std::vector<std::string> n2;
   dtdb::name_key_map::iterator itr = m_keyc->begin();
   while( !itr.end() )
   {
   rout( err, "VVVVVVVVVVVVVVVVVVV" );
   BOOST_FOREACH( std::string n, m_names )
    rout( err, "before %1%", %n );
    rout( err, "" );

    rout( err, "loaded name '%1%'", %itr.key() );
    std::string tmp = itr.key();
   rout( status, "tmp: %1%", %tmp );
        m_names.push_back(tmp);
        n2.push_back(tmp);
   BOOST_FOREACH( std::string n, m_names )
    rout( err, "after %1%", %n );
    rout( err, "" );
        ++itr;
        rout( err, "inc" );
   BOOST_FOREACH( std::string n, m_names )
    rout( err, "after %1%", %n );
    rout( err, "" );
   BOOST_FOREACH( std::string n, n2 )
    rout( err, "n2 %1%", %n );
    rout( err, "" );
   rout( err, "^^^^^^^^^^^^^^^^^^" );
   }
   BOOST_FOREACH( std::string n, m_names )
    rout( warn, "%1%", %n );
*/

    /*
    std::map<int,std::string> accounts;
    QSettings pref;
    pref.beginGroup( "Accounts" );
    QStringList acnts = pref.childGroups();
    for( uint32_t i = 0; i < acnts.size(); ++i )
    {
        pref.beginGroup( acnts[i] );
        accounts[pref.value( "index" ).toInt()] = acnts[i].toStdString();
        pref.endGroup();
    }
    std::map<int,std::string>::const_iterator b = accounts.begin();
    std::map<int,std::string>::const_iterator e = accounts.end();
    while( b != e )
    {
        m_names.push_back( b->second );
        ++b;
    }
    */
//    m_names =
}

AccountTableModel::~AccountTableModel()
{

}

int AccountTableModel::rowCount( const QModelIndex& p )const 
{ 
    return get_known_names().size();
}


QVariant AccountTableModel::data( const QModelIndex& i, int role )const
{
    if( !i.isValid() ) 
        return QVariant();
    if (i.row() > get_known_names().size() || i.row() < 0)
        return QVariant();

    if( role == Qt::DisplayRole )
        return QString( get_known_names()[i.row()].c_str() );
    return QVariant();
}
QVariant AccountTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if( role != Qt::DisplayRole )
        return QVariant();
    if( orientation == Qt::Horizontal && section == 0 ) 
        return QString( "Accounts" );
    return QVariant();
}



void AccountTableModel::addAccount( const QString& text )
{
    QSettings pref;
    pref.beginGroup( "Accounts" );
    if( !pref.contains( text ) )
    {
        pref.setValue( text + "/index", int(get_known_names().size()) );
        pref.sync();
        beginResetModel();
        rout( status, "known names size: %1%", %get_known_names().size() );
        get_known_names().push_back(text.toStdString());
        rout( status, "known names size: %1%", %get_known_names().size() );
        endResetModel();
    }
    pref.endGroup();
}
