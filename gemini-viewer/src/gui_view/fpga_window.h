#ifndef FPGA_WINDOW_H
#define FPGA_WINDOW_H

#include <QHostAddress>
#include <QString>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTableWidget>
#include <QTimer>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <memory>
#include "gemini_comms.h"

class Address_map;
struct Reg_block
{
    uint32_t base;
    uint32_t len;
    bool is_hole;
};

class Fpga_window : public QWidget
{
    Q_OBJECT

private:
    QHostAddress m_addr;
    uint16_t m_port;
    QString m_file;
    std::unique_ptr<Address_map> m_am;

    Gemini_comms *m_gemio;
    bool m_isConnected;
    RwChannel * m_update_ch; // for requesting regular updates
    RwChannel * m_writer_ch; // for writing to individual registers
    RwChannel * m_upload_ch; // used for bulk upload
    RwChannel * m_download_ch; // used for bulk download
    QVector<uint32_t> m_lastRegs;
    bool m_bold_changes; // show changed registers in bold

    QTextEdit *m_textEdit;
    QPushButton *m_upButton;
    QPushButton *m_dnButton;
    QLineEdit *m_lineEdit;
    QLineEdit *m_lineEdit2;
    //QLineEdit *m_status;
    QLabel *m_ackStatus;
    QLabel *m_failCode;
    QTreeWidget *m_treeWidget;
    QTableWidget *m_tableWidget;

    uint32_t m_tableBase;
    uint32_t m_lastTableBase;
    uint32_t m_numRegs;
    uint32_t m_lastNumRegs;
    uint32_t m_newBase;
    std::vector<Reg_block> m_reg_blks;
    uint32_t m_reg_blk_idx;

    QTimer *m_timeoutTimer;
    QTimer *m_dlyTimer;
    bool m_continueRunning;

    QString m_dl_filename;

    void closeEvent(QCloseEvent *event) override;
    void updateFromFPGA2();
    void add_remove_tablewidgets();
    void adjust_address_display();
    bool find_addr(QString &hdrline, uint32_t *base, uint32_t *offset);
    std::vector<Reg_block> get_reg_read_list(uint32_t idx);
    void print_reg_blks();

private slots:
    void onConnectResult(Gem_cnx_rslt rslt);
    void onCnxFailed();
    void onDelayDone();
    void onUpdate2Done(bool timeout, uint32_t base, uint32_t numRegs
                       , QByteArray regs, Gem_rw_type op, bool isAck
                       , uint8_t failCode);
    void onRegWriteDone(bool timeout, uint32_t base, uint32_t numRegs
                       , QByteArray regs, Gem_rw_type op, bool isAck
                       , uint8_t failCode);
    void onBaseAddrChange();
    void onLengthChange();
    void onTreeWidgetItemSelectionChanged();

    void onItemDoubleClicked(QTableWidgetItem *twi);
    void onUploadClicked(bool checked);
    void onDownloadClicked(bool checked);
    void onDownloadResult(bool timeout, uint32_t base, uint32_t numRegs
                       , QByteArray regs, Gem_rw_type op, bool isAck
                       , uint8_t failCode);

public:
    Fpga_window(Fpga_window& other) = delete; // no copy construct
    Fpga_window& operator=(Fpga_window& rhs) = delete; // no copy assign
    explicit Fpga_window(QHostAddress host, uint16_t port, QString file
                         , std::unique_ptr<Address_map> am
                         , QWidget *parent = 0);
    ~Fpga_window();
    bool isSameFPGA(QHostAddress addr, uint16_t port);
    QString getId();

signals:
    void closed(Fpga_window* win);

public slots:
};

#endif // FPGA_WINDOW_H
