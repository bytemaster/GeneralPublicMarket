#ifndef _TRX_TABLE_MODEL_HPP
#define _TRX_TABLE_MODEL_HPP
#include <QAbstractTableModel>
#include <vector>
#include <gpm/node/node.hpp>

class TrxTableModel : public QAbstractTableModel
{
    Q_OBJECT
    public:
        TrxTableModel( QObject* parent = 0 );
        ~TrxTableModel();

        int      rowCount(const QModelIndex &parent) const;
        int      columnCount(const QModelIndex &parent) const;
        QVariant data(const QModelIndex &index, int role) const;

        QVariant headerData(int section, Qt::Orientation orientation, int role) const;
//        Qt::ItemFlags flags(const QModelIndex &index) const;

        void setTrxLog( const std::string& acnt, const std::vector<gpm::trx_log>& tl )
        {
            beginResetModel();
            m_acnt = acnt;
            m_trx_log =  tl;
            endResetModel();
        }
        private:
            std::string           m_acnt;
            std::vector<gpm::trx_log>  m_trx_log;
//    private:
//        std::vector<std::pair<QString,uint64_t> > m_names;

};


#endif
