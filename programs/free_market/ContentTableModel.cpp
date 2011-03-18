#include "ContentTableModel.hpp"

ContentTableModel::ContentTableModel( QObject* p )
:QAbstractTableModel(p)
{
}

ContentTableModel::~ContentTableModel()
{

}

int ContentTableModel::rowCount( const QModelIndex& p )const { return m_names.size(); }

QVariant ContentTableModel::data( const QModelIndex& i, int role )const
{
    if( !i.isValid() ) 
        return QVariant();
    if (i.row() >= m_names.size() || i.row() < 0)
        return QVariant();

    if( role == Qt::DisplayRole )
    {
        if( i.column() == 0 )
            return m_names[i.row()].first;
        else
            return m_names[i.row()].second;
    }
    return QVariant();
}
QVariant ContentTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if( role != Qt::DisplayRole )
        return QVariant();
    if( orientation == Qt::Horizontal  ) 
    {
        if( section == 0 )
            return QString( "Type" );
        if( section ==1 )
            return QString( "Balance" );
    }
    return QVariant();
}
