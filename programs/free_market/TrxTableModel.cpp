#include "TrxTableModel.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>

TrxTableModel::TrxTableModel( QObject* p )
:QAbstractTableModel(p)
{
}

TrxTableModel::~TrxTableModel()
{

}

int TrxTableModel::columnCount(const QModelIndex &parent) const
{
    // source/dst 
    // type
    // amnt
    // balance
    // date
    return 5;
}
int TrxTableModel::rowCount( const QModelIndex& p )const 
{ 
    return m_trx_log.size();
}

/*
template<typename T>
std::string to_string( const T& v )
{
    std::stringstream ss; 
    ss<<v;
    return v.str();
}
*/

QVariant TrxTableModel::data( const QModelIndex& i, int role )const
{
    static boost::posix_time::ptime epoch(boost::gregorian::date(1970, boost::gregorian::Jan, 1));
    if( !i.isValid() ) 
        return QVariant();
    if (i.row() >= m_trx_log.size() || i.row() < 0)
        return QVariant();

    if( role == Qt::DisplayRole )
    {
        switch( i.column() )
        {
            case 0:
               return m_acnt == m_trx_log[i.row()].from ? QString(m_trx_log[i.row()].to.c_str())  : QString( m_trx_log[i.row()].from.c_str() );  
            case 1:
                return QString(m_trx_log[i.row()].type.c_str());
            case 2:
                return QString( m_trx_log[i.row()].to == m_acnt ? "" : "-" ) + QString::number( m_trx_log[i.row()].amnt );
            case 3:
                 return  m_trx_log[i.row()].from == m_acnt ? m_trx_log[i.row()].from_bal  : m_trx_log[i.row()].to_bal;
            case 4:
                 return QString( to_string( (epoch + boost::posix_time::seconds(m_trx_log[i.row()].date/1000000) ) ).c_str() );

        }
    }
    return QVariant();
}
QVariant TrxTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if( role != Qt::DisplayRole )
        return QVariant();
    if( orientation == Qt::Horizontal  ) 
    {
        if( section == 0 )
            return QString( "Source/Dest" );
        else if( section ==1 )
            return QString( "Type" );
        else if( section ==2 )
            return QString( "Amount" );
        else if( section ==3 )
            return QString( "Balance" );
        else if( section ==4 )
            return QString( "Date" );
    }
    return QVariant();
}
