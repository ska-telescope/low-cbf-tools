#include "download_dialog.h"
#include <QLabel>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>

Download_dialog::Download_dialog(QWidget *parent)
    : QDialog(parent)
    , m_reg_name_valid(false)
    , m_offset(0)
    , m_offset_valid(true)
    , m_length(1)
    , m_length_valid(true)
    , m_filename("download.dat")
    , m_filename_valid(true)
{
    setWindowTitle("Download");

    QLabel *lbl_1 = new QLabel("Register:");
    m_register_name = new QLineEdit("system.system.xyz_testbram");
    QHBoxLayout *hl_1 = new QHBoxLayout();
    hl_1->addWidget(lbl_1);
    hl_1->addStretch();
    hl_1->addWidget(m_register_name);

    QLabel *lbl_2 = new QLabel("Offset:");
    QString txt = QString("0x%1").arg(m_offset);
    m_register_offset = new QLineEdit(txt);
    QHBoxLayout *hl_2 = new QHBoxLayout();
    hl_2->addWidget(lbl_2);
    hl_2->addStretch();
    hl_2->addWidget(m_register_offset);

    QLabel *lbl_3 = new QLabel("Length:");
    txt = QString("0x%1").arg(m_length);
    m_download_length = new QLineEdit(txt);
    QHBoxLayout *hl_3 = new QHBoxLayout();
    hl_3->addWidget(lbl_3);
    hl_3->addStretch();
    hl_3->addWidget(m_download_length);

    QLabel *lbl_4 = new QLabel("Filename");
    m_filename_te = new QLineEdit(m_filename);
    m_filename_valid = true;
    QHBoxLayout *hl_4 = new QHBoxLayout();
    hl_4->addWidget(lbl_4);
    hl_4->addStretch();
    hl_4->addWidget(m_filename_te);


    QVBoxLayout *vl_1 = new QVBoxLayout();
    vl_1->addLayout(hl_1);
    vl_1->addLayout(hl_2);
    vl_1->addLayout(hl_3);
    vl_1->addLayout(hl_4);
    vl_1->addStretch();

    QPushButton *pb_1 = new QPushButton;
    pb_1->setText("OK");
    QPushButton *pb_2 = new QPushButton;
    pb_2->setText("Cancel");

    QVBoxLayout *vl_2 = new QVBoxLayout;
    vl_2->addWidget(pb_1);
    vl_2->addWidget(pb_2);
    vl_2->addStretch();

    QHBoxLayout *hl_5 = new QHBoxLayout();
    hl_5->addLayout(vl_1);
    hl_5->addLayout(vl_2);
    setLayout(hl_5);

    connect(pb_1, &QPushButton::clicked, this, &Download_dialog::accept);
    connect(pb_2, &QPushButton::clicked, this, &Download_dialog::reject);
    connect(m_register_name, &QLineEdit::editingFinished
            , this, &Download_dialog::on_edit_name);
    connect(m_register_offset, &QLineEdit::editingFinished
            , this, &Download_dialog::on_edit_offset);
    connect(m_download_length, &QLineEdit::editingFinished
            , this, &Download_dialog::on_edit_length);
    connect(m_filename_te, &QLineEdit::editingFinished
            , this, &Download_dialog::on_edit_filename);
}

void Download_dialog::on_edit_name()
{
    QString txt = m_register_name->text();
    //TODO handle invalid filename??
    m_reg_name = txt;
    m_reg_name_valid = (m_reg_name.length() != 0);
}

void Download_dialog::on_edit_offset()
{
    QString txt = m_register_offset->text();
    bool ok;
    int val = txt.toUInt(&ok,0);
    if(ok && (val>0) && (val<=65535))
    {
        m_offset = val;
        m_offset_valid = true;
    }
    else
        m_offset_valid = false;
}
void Download_dialog::on_edit_length()
{
    QString txt = m_download_length->text();
    bool ok;
    int val = txt.toUInt(&ok,0);
    if(ok && (val>0) && (val<=65535))
    {
        m_length = val;
        m_length_valid = true;
    }
    else
        m_length_valid = false;
}

void Download_dialog::on_edit_filename()
{
    QString txt = m_filename_te->text();
    //TODO handle invalid filename??
    m_filename = txt;
    m_filename_valid = (m_filename.length() != 0);
}

QString Download_dialog::get_reg_name()
{
    return m_reg_name;
}

uint32_t Download_dialog::get_reg_offset()
{
    return m_offset;
}

uint32_t Download_dialog::get_length()
{
    return m_length;
}

QString Download_dialog::get_file_name()
{
    return m_filename;
}

bool Download_dialog::is_valid()
{
    return (m_reg_name_valid
            && m_offset_valid
            && m_length_valid
            && m_filename_valid);
}
