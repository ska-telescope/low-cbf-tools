#ifndef OPEN_FPGA_DLG_H
#define OPEN_FPGA_DLG_H

/* This dialog is presented to the user when they want to open an FPGA to
 * see the contents of its registers.
 *
 * The user is asked for the IP address and port to use when communicating
 * with the FPGA. They are also asked for the "fpgamap.py" file that describes
 * the registers and their content. The file is output by ARGS
 * (Automatic Register Generation System) which is part of RadioHDL.
 */

#include <QDialog>
#include <QHostAddress>
#include <QString>
#include <QLineEdit>

class Open_fpga_dlg: public QDialog
{
    Q_OBJECT

    private:
        QHostAddress m_addr;
        uint16_t m_port;
        QString m_filename;

        bool m_valid_addr;
        bool m_valid_port;
        bool m_valid_filename;

        QLineEdit * m_host_led;
        QLineEdit * m_port_led;
        QLineEdit * m_filename_led;
    private slots:
        void onEditAddress();
        void onEditPort();
        void onEditFilename();
        void onClickFileFind();
    public:
        Open_fpga_dlg(QHostAddress addr, uint16_t port, QString filename);
        // Accessors used to get information about the user's choices after
        // the user has edited and accepted the dialog
        QHostAddress get_host();
        uint16_t get_port();
        QString get_filename();
        bool isValid();
};

#endif
