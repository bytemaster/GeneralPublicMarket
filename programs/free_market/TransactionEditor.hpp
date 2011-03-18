#ifndef _TRANSACTION_EDITOR_HPP_
#define _TRANSACTION_EDITOR_HPP_
#include <QWidget>
#include <boost/filesystem.hpp>

class Ui_TransactionEditor;

class TransactionEditor : public QWidget
{
    Q_OBJECT
    public:
        TransactionEditor( QWidget* parent = NULL );


    public Q_SLOTS:
        void save();
        void submit();

        void addCommand();
        void updateSignatures();

    Q_SIGNALS:

    private:
        Ui_TransactionEditor*    ui;
        boost::filesystem::path  m_filename;

};

#endif
