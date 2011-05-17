#ifndef _MAINWINDOW_HPP_
#define _MAINWINDOW_HPP_
#include <QMainWindow>
#include <gpm/node/node.hpp>
#include <QItemSelection>
#include "keychain.hpp"

class Ui_main_window;
class AccountTableModel;
class ContentTableModel;
class TrxTableModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    public:
        MainWindow();
        ~MainWindow();

    public Q_SLOTS:
        void accountSelectionChanged( const QItemSelection & selected, const QItemSelection & deselected ); 
        void contentSelectionChanged( const QItemSelection & selected, const QItemSelection & deselected ); 

        void newAccount();
        void newTransaction();
        void configureGeneration();
        void generate();

    private:
        std::string  m_selected_acnt;
        std::string  m_selected_type;

        Ui_main_window* ui;
        AccountTableModel*     atm;
        ContentTableModel*     ctn;
        TrxTableModel*         trx;
};

#endif
