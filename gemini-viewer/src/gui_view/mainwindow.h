#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QCloseEvent>

#include "pub_client.h"
#include "event_model.h"
#include "fpga_window.h"

class MainWindow: public QMainWindow
{
    Q_OBJECT

    private:
        QTextEdit *m_text_edit;
        Pub_client *m_pubclient;  // Listens for UDP publish packets
        Event_model *m_eventmodel; // Gemini card information table
        QList<Fpga_window *> m_fpga_windows;

        void closeEvent(QCloseEvent *event) override;
        void openFPGA(QHostAddress addr, uint16_t port, QString file);
    private slots:
        void helpAbout();
        void onOpenFPGA_fromMenu();
        void onOpenFPGA_fromModel(const QModelIndex &index);
        void onFpgaWinClose(Fpga_window* win);
    public slots:

    public:

        MainWindow();
        MainWindow(const MainWindow&) = delete; // no copy construct
        MainWindow& operator=(const MainWindow&) = delete; // no copy assign
        ~MainWindow();
    signals:
        void done();
};

#endif
