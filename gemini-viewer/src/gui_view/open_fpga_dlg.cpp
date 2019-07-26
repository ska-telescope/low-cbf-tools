#include "open_fpga_dlg.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QStyle>
#include <QApplication>

Open_fpga_dlg::Open_fpga_dlg(QHostAddress addr, uint16_t port, QString filename)
    : m_addr(addr)
    , m_port(port)
    , m_filename(filename)
    , m_valid_addr(true)
    , m_valid_port(true)
    , m_valid_filename(true)
{
    setWindowTitle("Open FPGA");

    m_valid_filename = (filename.length() != 0);
    m_valid_addr = !addr.isNull();
    // m_valid_port always true since parameter is 16-bit unsigned

    QLabel *lbl_1 = new QLabel;
    lbl_1->setText("Host:");
    m_host_led = new QLineEdit;
    m_host_led->setText(addr.toString());
    QHBoxLayout * hl_1 = new QHBoxLayout;
    hl_1->addWidget(lbl_1);
    hl_1->addStretch();
    hl_1->addWidget(m_host_led);

    QLabel *lbl_2 = new QLabel;
    lbl_2->setText("Port:");
    m_port_led = new QLineEdit;
    m_port_led->setText(QString("%1").arg(port));
    QHBoxLayout * hl_2 = new QHBoxLayout;
    hl_2->addWidget(lbl_2);
    hl_2->addStretch();
    hl_2->addWidget(m_port_led);
 

    QLabel *lbl_3 = new QLabel(this);
    lbl_3->setText("fpga map:");
    m_filename_led = new QLineEdit;
    m_filename_led->setText(filename);
    QPushButton *pb_file = new QPushButton(this);
    pb_file->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
    connect(pb_file, &QPushButton::clicked, this, &Open_fpga_dlg::onClickFileFind);
    QHBoxLayout * hl_3 = new QHBoxLayout;
    hl_3->addWidget(lbl_3);
    hl_3->addWidget(pb_file);
    hl_3->addStretch();
    hl_3->addWidget(m_filename_led);


    QVBoxLayout *vl_1 = new QVBoxLayout;
    vl_1->addLayout(hl_1);
    vl_1->addLayout(hl_2);
    vl_1->addLayout(hl_3);

    QPushButton *pb_1 = new QPushButton;
    pb_1->setText("OK");
    QPushButton *pb_2 = new QPushButton;
    pb_2->setText("Cancel");

    QVBoxLayout *vl_2 = new QVBoxLayout;
    vl_2->addWidget(pb_1);
    vl_2->addWidget(pb_2);
    vl_2->addStretch();

    QHBoxLayout *hl_4 = new QHBoxLayout;
    hl_4->addLayout(vl_1);
    hl_4->addStretch();
    hl_4->addLayout(vl_2);

    setLayout(hl_4);

    connect(pb_1, &QPushButton::clicked, this, &Open_fpga_dlg::accept);
    connect(pb_2, &QPushButton::clicked, this, &Open_fpga_dlg::reject);

    connect(m_host_led, &QLineEdit::editingFinished
            , this, &Open_fpga_dlg::onEditAddress);
    connect(m_port_led, &QLineEdit::editingFinished
            , this, &Open_fpga_dlg::onEditPort);
    connect(m_filename_led, &QLineEdit::editingFinished
            , this, &Open_fpga_dlg::onEditFilename);
}

void Open_fpga_dlg::onEditAddress()
{
    QString txt = m_host_led->text();
    QHostAddress addr = QHostAddress(txt);
    if( addr.isNull())
    {
        m_valid_addr = false;
        return;
    }
    m_addr = addr;
    m_valid_addr = true;
}

void Open_fpga_dlg::onEditPort()
{
    QString txt = m_port_led->text();
    bool ok;
    int val = txt.toInt(&ok);
    if(ok && (val>0) && (val<=65535))
    {
        m_port = val;
        m_valid_port = true;
    }
    else
        m_valid_port = false;
}

void Open_fpga_dlg::onEditFilename()
{
    QString txt = m_filename_led->text();
    //TODO handle invalid filename??
    m_filename = txt;
    m_valid_filename = (m_filename.length() != 0);
}

void Open_fpga_dlg::onClickFileFind()
{
    QString fname = QFileDialog::getOpenFileName(this
                         , "config file", "", "c_config (*.ccfg)");
    if( fname.length() != 0)
    {
        m_filename_led->setText(fname);
        m_filename = fname;
        m_valid_filename = (fname.length() != 0);
    }
}

QHostAddress Open_fpga_dlg::get_host()
{
    return m_addr;
}

uint16_t Open_fpga_dlg::get_port()
{
    return m_port;
}

QString Open_fpga_dlg::get_filename()
{
    return m_filename;
}

bool Open_fpga_dlg::isValid()
{
    return(m_valid_addr && m_valid_port && m_valid_filename);
}
