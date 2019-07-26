#ifndef DOWNLOAD_DIALOG_H
#define DOWNLOAD_DIALOG_H

#include <QObject>
#include <QDialog>
#include <QLineEdit>

class Download_dialog : public QDialog
{
    Q_OBJECT

private:
    QLineEdit * m_register_name;
    QString m_reg_name;
    bool m_reg_name_valid;

    QLineEdit * m_register_offset;
    uint32_t m_offset;
    bool m_offset_valid;

    QLineEdit * m_download_length;
    uint32_t m_length;
    bool m_length_valid;

    QLineEdit * m_filename_te;
    QString m_filename;
    bool m_filename_valid;

private slots:
    void on_edit_name();
    void on_edit_offset();
    void on_edit_length();
    void on_edit_filename();
public:
    explicit Download_dialog(QWidget *parent = 0);

    QString get_reg_name();
    uint32_t get_reg_offset();
    uint32_t get_length();
    QString get_file_name();
    bool is_valid();

signals:

public slots:
};

#endif // DOWNLOAD_DIALOG_H
