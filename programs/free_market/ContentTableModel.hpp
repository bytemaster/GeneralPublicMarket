#ifndef _CONTENT_TABLE_MODEL_HPP
#define _CONTENT_TABLE_MODEL_HPP
#include <QAbstractTableModel>
#include <vector>

class ContentTableModel : public QAbstractTableModel
{
    Q_OBJECT
    public:
        ContentTableModel( QObject* parent = 0 );
        ~ContentTableModel();

        int      rowCount(const QModelIndex &parent) const;
        int      columnCount(const QModelIndex &parent) const { return 2; }
        QVariant data(const QModelIndex &index, int role) const;

        QVariant headerData(int section, Qt::Orientation orientation, int role) const;

        void setData( const std::vector<std::pair<QString,uint64_t> >& d )
        {
            beginResetModel();
            m_names = d;
            endResetModel();
            return;
            if( d.size() > m_names.size() )
            {
                beginInsertRows( QModelIndex(), m_names.size(), d.size()-1 );
                m_names = d;
                endInsertRows();
            }
            else if( m_names.size() < d.size() )
            {
                //rout( status, "REMOVING ROWS %1%  %2%", %d.size() %(m_names.size()-1) );
                beginRemoveRows( QModelIndex(), d.size(), m_names.size()-1 );
                m_names = d;
                endRemoveRows();
            }
            else
            {
                m_names = d;
            }
            Q_EMIT dataChanged( index( 0,0 ), index( m_names.size()-1, 1 ) );
        }
//        Qt::ItemFlags flags(const QModelIndex &index) const;

        std::string getType(int i ){ return m_names[i].first.toStdString(); }
     private:
        std::vector<std::pair<QString,uint64_t> > m_names;

};


#endif
