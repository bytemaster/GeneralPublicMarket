#ifndef _TRANSFER_FORM_HPP_
#define _TRANSFER_FORM_HPP_
#include <QWidget>
#include "AbstractCommandForm.hpp"

class Ui_TransferForm;

class TransferForm : public QWidget, virtual public AbstractCommandForm
{
    Q_OBJECT
    public:
        TransferForm( QWidget* parent );

        void createCommand( gpm::command& cmd )const;
        std::vector<std::string> getErrors()const;
        std::vector<std::string> getRequiredSignatures()const;

    public Q_SLOTS:
        void currentFromChanged( const QString& txt );

    private:
    Ui_TransferForm* m_ui;
};

#endif
