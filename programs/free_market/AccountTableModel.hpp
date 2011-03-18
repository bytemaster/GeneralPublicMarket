#ifndef _ACCOUNT_TABLE_MODEL_HPP
#define _ACCOUNT_TABLE_MODEL_HPP
#include <QAbstractTableModel>
#include <vector>
#include <dtdb/node.hpp>
#include "keychain.hpp"


/**
 *  Accounts that are viewed are stored in the system preferences.  A user does not
 *  need to have the keys to an account to view it.  This is done intentionally because 
 *  we do not want users to think that the information is in any way 'private'.
 *  
 *  If they want privacy then they need to keep their account names separate from their
 *  real world identity.
 *  
 *  Adding an account will update preferences. 
 *
 */
class AccountTableModel : public QAbstractTableModel
{
    Q_OBJECT
    public:
        AccountTableModel( QObject* p );
        ~AccountTableModel();

        void     addAccount( const QString& acnt );

        int      rowCount(const QModelIndex &parent) const;
        int      columnCount(const QModelIndex &parent) const { return 1; }
        QVariant data(const QModelIndex &index, int role) const;
        //bool insertRows( int p, int r, const QModelIndex& i = QModelIndex() );

        QVariant headerData(int section, Qt::Orientation orientation, int role) const;
//        Qt::ItemFlags flags(const QModelIndex &index) const;
        std::string getName( uint32_t i ) { return get_known_names()[i]; }
    private:
//        std::vector<std::string>  m_names;

};


#endif
