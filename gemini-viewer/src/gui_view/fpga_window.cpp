#include "fpga_window.h"
#include "gemini_comms.h"
#include "address_map.h"
#include "download_dialog.h"
#include <QDebug>
#include <QCloseEvent>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QInputDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QVector>


#define GEM_TIMEOUT_MSEC 500
#define GEM_RETRIES 3

Fpga_window::Fpga_window(QHostAddress host, uint16_t port, QString file
                         , std::unique_ptr<Address_map> am, QWidget *parent)
    : QWidget(parent)
    , m_addr(host)
    , m_port(port)
    , m_file(file)
    , m_am(std::move(am))
    //, m_am(std::forward<std::unique_ptr<Address_map>>(am))
    , m_gemio(nullptr)
    , m_isConnected(false)
    , m_update_ch(nullptr)
    , m_writer_ch(nullptr)
    , m_upload_ch(nullptr)
    , m_download_ch(nullptr)
    , m_bold_changes(false)
    , m_tableBase(0)
    , m_lastTableBase(0)
    , m_numRegs(0)
    , m_lastNumRegs(0)
    , m_continueRunning(true)
{
    QString titleTxt = QString("FPGA %1:%2 [%3]")
                                .arg(host.toString()).arg(port).arg(file);
    setWindowTitle(titleTxt);
    this->setMinimumWidth(400);
    this->resize(400,600);

    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    m_dlyTimer = new QTimer(this);
    m_dlyTimer->setSingleShot(true); 


    m_lineEdit = new QLineEdit();
    m_lineEdit->setText(QString("0x0"));
    connect(m_lineEdit, &QLineEdit::editingFinished, this, &Fpga_window::onBaseAddrChange);

    m_lineEdit2 = new QLineEdit();
    m_lineEdit2->setText("0");
    connect(m_lineEdit2, &QLineEdit::editingFinished, this, &Fpga_window::onLengthChange);

    QLabel *lbl_1 = new QLabel();
    lbl_1->setText("BaseAddr:");
    QLabel *lbl_2 = new QLabel();
    lbl_2->setText("Length:");
    m_upButton = new QPushButton("&Upload", this);
    connect(m_upButton, &QPushButton::clicked
            , this, &Fpga_window::onUploadClicked);
    //m_upButton->setText("Upload");
    m_dnButton = new QPushButton("&Download", this);
    connect(m_dnButton, &QPushButton::clicked
            , this, &Fpga_window::onDownloadClicked);
    //m_dnButton->setText("Capture");

    QHBoxLayout *hl_1 = new QHBoxLayout;
    hl_1->addWidget(lbl_1);
    hl_1->addWidget(m_lineEdit);
    hl_1->addWidget(lbl_2);
    hl_1->addWidget(m_lineEdit2);
    hl_1->addStretch();
    hl_1->addWidget(m_upButton);
    hl_1->addWidget(m_dnButton);

    // Tree of peripherals and ports
    m_treeWidget = new QTreeWidget();
    m_treeWidget->setColumnCount(2);
    m_treeWidget->setHeaderLabels(QStringList({QString("peripheral"), QString("base")}));
    QList<QTreeWidgetItem *> items;
    QString prev_periph_name;
    QTreeWidgetItem * prev_item = nullptr;
    for(int i=0; i<m_am->get_num_ports(); i++)
    {
        Axi_port_t * axi_port = m_am->get_port(i);
        QString port_name = axi_port->port_name;
        QString peripheral_name = axi_port->peripheral_name;
        // Start new peripheral block
        if(peripheral_name != prev_periph_name)
        {
            QStringList cols;
            cols << peripheral_name;
            cols << QString("");
            QTreeWidgetItem *itm = new QTreeWidgetItem( (QTreeWidget*)0, cols, -1);
            m_treeWidget->addTopLevelItem(itm);
            prev_item = itm;
            prev_periph_name = peripheral_name;
        }
        // Add port to peripheral
        QStringList cols;
        cols << port_name;
        cols << QString("0x%1").arg(axi_port->base_addr, 8, 16, QLatin1Char('0'));
        QTreeWidgetItem *itm = new QTreeWidgetItem( (QTreeWidgetItem*)0, cols, i);
        prev_item->addChild(itm);
    }
    connect(m_treeWidget, &QTreeWidget::itemSelectionChanged
            , this, &Fpga_window::onTreeWidgetItemSelectionChanged);

    if(m_am->get_num_ports() <= 0)
    {
        // if config file has no content, add in one register to allow the gui to be used manually
        m_tableBase = 0;
        m_numRegs = 1;

        Reg_block rb;
        rb.base = m_tableBase;
        rb.len = m_numRegs;
        rb.is_hole = false;
        m_reg_blks.push_back(rb);
    }
    else
    {
        m_tableBase = m_am->get_port(0)->base_addr;
        m_numRegs = m_am->get_port(0)->n_regs;
        m_reg_blks = get_reg_read_list(0);
    }
    m_lineEdit->setText(QString("0x%1").arg(m_tableBase,8,16,QLatin1Char('0')));

    //print_reg_blks();

    // Table for register values
    m_tableWidget = new QTableWidget();
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->verticalHeader()->hide();
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    // We want 3 columns that fit into the window size
    m_tableWidget->setColumnCount(3);
    QStringList columnNames = {"Register", "Address", "Value" };
    m_tableWidget->setHorizontalHeaderLabels(columnNames);
    add_remove_tablewidgets();
    m_lineEdit2->setText(QString("%1").arg(m_numRegs));
#if 0
    m_tableWidget->setRowCount(m_numRegs);
    for(uint32_t row=0; row<m_numRegs; row++)
    {
        // Second column items
        QTableWidgetItem *tItem = new QTableWidgetItem();
        tItem->setText( QString("0x%1").arg(m_tableBase+row,8,16
                    ,QLatin1Char('0')));
        tItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        tItem->setFlags( Qt::NoItemFlags); // no select, edit, etc
        QBrush b;
        b.setColor(Qt::black);
        tItem->setForeground(b);
        m_tableWidget->setItem(row, 1, tItem);

        // Third column items
        tItem = new QTableWidgetItem();
        tItem->setText( QString("??"));
        tItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        tItem->setFlags( Qt::NoItemFlags); // no select, edit, etc
        b.setColor(Qt::blue);
        tItem->setForeground(b);
        m_tableWidget->setItem(row, 2, tItem);
    }
#endif

#if 0
    // Text box for display of random output
    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setUndoRedoEnabled(false);
    m_textEdit->setMinimumHeight(100);
    m_textEdit->setDocumentTitle("Output");
    m_textEdit->document()->setMaximumBlockCount(100); // limit length
#endif

    //m_status = new QLineEdit();
    m_ackStatus = new QLabel();
    m_failCode = new QLabel();
    QLabel * namelbl = new QLabel();
    namelbl->setText("FailCode:");
    QHBoxLayout *hl_2 = new QHBoxLayout();
    hl_2->addWidget(m_ackStatus);
    hl_2->addStretch();
    hl_2->addWidget(namelbl);
    hl_2->addWidget(m_failCode);

    QHBoxLayout *hl_3 = new QHBoxLayout();
    hl_3->addWidget(m_treeWidget);
    hl_3->addWidget(m_tableWidget);

    QVBoxLayout * vl_1 = new QVBoxLayout;
    vl_1->addLayout(hl_1);
    //vl_1->addWidget(m_tableWidget);
    vl_1->addLayout(hl_3);
    //vl_1->addWidget(m_textEdit);
    //vl_1->addWidget(m_status);
    vl_1->addLayout(hl_2);
    setLayout(vl_1);

    m_lastRegs.reserve(2048>m_numRegs?2048:m_numRegs);
    for(unsigned int i=0; i<m_numRegs; i++)
        m_lastRegs[i] = 0;

    // double-clicks cause write to register
    connect(m_tableWidget, &QTableWidget::itemDoubleClicked
            , this, &Fpga_window::onItemDoubleClicked);

    m_gemio = new Gemini_comms(host, port, GEM_TIMEOUT_MSEC, GEM_RETRIES);
    connect(m_gemio, &Gemini_comms::cnx_result
            , this, &Fpga_window::onConnectResult);
    m_gemio->udpConnect();
}

Fpga_window::~Fpga_window()
{
    qDebug().nospace().noquote() << "Deleting Fpga_window " << m_addr.toString()
                                 << ":" << m_port;

    m_dlyTimer->stop();
    delete m_dlyTimer;
    m_dlyTimer = nullptr;

    m_timeoutTimer->stop();
    delete m_timeoutTimer;
    m_timeoutTimer = nullptr;

#if 0
    // Take all items out of table widget and delete them individually
    // This seems to stop memory leakage if fpga_window is closed & reopened
    //   as if the TableWidget doesn't own the TableWidgetItems
    for(int row=0; row<m_tableWidget->rowCount(); row++)
    {
        for(int col=0; col<m_tableWidget->columnCount(); col++)
        {
            QTableWidgetItem * twi = m_tableWidget->item(row,col);
            if(twi)
            {
                m_tableWidget->takeItem(row, col);
                delete twi;
            }
        }
    }
#endif

    // TODO: what else to clean up here ?
    if(m_download_ch)
        m_download_ch->dispose();
    if(m_upload_ch)
        m_upload_ch->dispose();
    if(m_writer_ch)
        m_writer_ch->dispose();
    if(m_update_ch)
        m_update_ch->dispose();
    if(m_gemio)
        m_gemio->deleteLater();
}

void Fpga_window::onConnectResult(Gem_cnx_rslt rslt)
{
    m_isConnected = (rslt == Gem_cnx_rslt::OK);
    if(!m_isConnected)
    {
        QMessageBox msg;
        QString txt;
        switch(rslt)
        {
        case Gem_cnx_rslt::TIMEOUT:
            txt = QString("Timeout connecting to FPGA");
            break;
        case Gem_cnx_rslt::FAIL_TEMP:
            txt = QString("Temporary failure connecting to FPGA."
                          " Too many clients");
            break;
        case Gem_cnx_rslt::FAIL_PERM:
            txt = QString("Permanent failure connecting to FPGA");
            break;
        default:
            txt = QString("Unknown error connecting to FPGA");
            break;
        }
        msg.setText(txt);
        msg.setIcon(QMessageBox::Critical);
        // Would like to set dialog near MainWindow, but this window is
        // a top-level window (no parent) so short of passing in a refernece
        // to the MainWindow there isn't an easy way to get its position ...
        // Just let the position go to the default centre of screen.
        //QPoint posn = this->mapToParent(QPoint(40,100));
        //msg.move(posn);
        msg.exec();
        this->close();
        return;
    }

    this->show();

    connect(m_gemio, &Gemini_comms::cnx_failed
            , this, &Fpga_window::onCnxFailed);
    connect(m_dlyTimer, &QTimer::timeout
            , this, &Fpga_window::onDelayDone);


    m_update_ch = m_gemio->openChannel();
    connect(m_update_ch, &RwChannel::result, this, &Fpga_window::onUpdate2Done);
    m_writer_ch = m_gemio->openChannel();
    connect(m_writer_ch, &RwChannel::result, this, &Fpga_window::onRegWriteDone);
    m_upload_ch = m_gemio->openChannel();
    // ignore result TODO is this always OK? connect to progress indicator??
    m_download_ch = m_gemio->openChannel();
    connect(m_download_ch, &RwChannel::result, this, &Fpga_window::onDownloadResult);

    updateFromFPGA2();
}

bool Fpga_window::isSameFPGA(QHostAddress addr, uint16_t port)
{
    return (addr == m_addr) && (port == m_port);
}

// Override close event so we can notify others that we're closed
void Fpga_window::closeEvent(QCloseEvent *event)
{
    m_continueRunning = false;
    event->accept();
    //qDebug().nospace().noquote() << "User close on FPGA "
    //                             << m_addr.toString() << ":" << m_port;
    emit closed(this);  // tell parent that window got close and is hidden
}

QString Fpga_window::getId()
{
    return QString("%1:%2").arg(m_addr.toString()).arg(m_port);
}

void Fpga_window::updateFromFPGA2()
{
    if(m_continueRunning)
    {
        if((m_tableBase != m_lastTableBase) || (m_numRegs != m_lastNumRegs))
        {
            add_remove_tablewidgets();
            adjust_address_display();
            m_lastTableBase = m_tableBase;
            m_lastNumRegs = m_numRegs;
            //qDebug().nospace().noquote() << "Region changed, now base=0x" << hex
            //        << QString("%1").arg(m_tableBase) << dec << " len=" << m_numRegs;
            m_bold_changes = false;
        }
        m_reg_blk_idx = 0;
        while(m_reg_blks.at(m_reg_blk_idx).is_hole)
        {
            ++m_reg_blk_idx;
        }


        //m_update_ch->rw(m_tableBase, m_numRegs,nullptr, Gem_rw_type::READ_INC);
        //QString txt1 = QString("Orig read from 0x%1, len 0x%2")
        //        .arg(m_tableBase, 0, 16)
        //        .arg(m_numRegs, 0, 16);
        //qDebug().noquote() << txt1;
        m_update_ch->rw(m_reg_blks.at(m_reg_blk_idx).base
                        , m_reg_blks.at(m_reg_blk_idx).len
                        ,nullptr, Gem_rw_type::READ_INC);
        //  will emit "result" which is connected to "onUpdate2Done"
        //QString txt = QString("Read from 0x%1, len 0x%2")
        //        .arg(m_reg_blks.at(m_reg_blk_idx).base, 0, 16)
        //        .arg(m_reg_blks.at(m_reg_blk_idx).len, 0, 16);
        //qDebug().noquote() << txt;
    }
}

void Fpga_window::onUpdate2Done(bool timeout, uint32_t base, uint32_t numRegs
                                , QByteArray regs, Gem_rw_type op
                                , bool isAck, uint8_t failCode)
{

    //Q_UNUSED(op);
    //Q_UNUSED(base);
    //Q_UNUSED(numRegs);

    if(timeout)
    {
        m_ackStatus->setText("Timeout");
        m_failCode->setText("");
        // Show that no data was received
        for(uint32_t i=0; i<m_numRegs; i++)
        {
            QTableWidgetItem * twi = m_tableWidget->item(i,2);
            twi->setText(QString("?"));
        }

        m_dlyTimer->start(500); // connected to "onDelayDone"
        return;
    }

    m_failCode->setText( QString("0x%1").arg(failCode, 2,16,QLatin1Char('0')));

    if(!isAck)
    {
        m_ackStatus->setText("[NACK]");
        for(int i=0; i<(int)m_numRegs; i++)
        {
            QTableWidgetItem * twi = m_tableWidget->item(i,2);
            twi->setText(QString("NACK"));
            QFont f;
            f.setBold(false);
            twi->setFont(f);
        }
        m_dlyTimer->start(500); // connected to "onDelayDone"
        return;
    }

    m_ackStatus->setText("  [ACK]");
    // Uploads will generate replys - we don't want to show them here
    if(op != Gem_rw_type::READ_INC)
    {
        m_dlyTimer->start(500); // connected to "onDelayDone"
        return;
    }

    int pkt_offset = (base-m_tableBase);
    if(m_lastRegs.size() < int32_t(pkt_offset+numRegs))
    {
        int prev_size = m_lastRegs.size();
        qDebug() << "expand m_lastRegs to" << pkt_offset+numRegs+1;
        m_lastRegs.reserve(pkt_offset+numRegs+1);
        qDebug() << ".. m_lastRegs expanded";
        if(prev_size < pkt_offset)
        {
            for(int i=prev_size; i< pkt_offset; i++)
                m_lastRegs.append(0xdead0000);
            qDebug() << ".. m_lastRegs hole filled";
        }
        for(int i=int(pkt_offset); i<int32_t(pkt_offset+numRegs+1); i++)
            m_lastRegs.append(0);
        qDebug() << ".. m_lastRegs expansion zeroed!";
    }


    for(int i=0; i<(int)numRegs; i++)
    {
        if((i+pkt_offset) >= m_tableWidget->rowCount())
        {
            qDebug() << "Skipping reg" << i+pkt_offset << "not in table"
                     << "base=" << base << "m_tablebase=" << m_tableBase;
            continue;
        }

        QTableWidgetItem * twi = m_tableWidget->item(i+pkt_offset,2);
        if(twi == nullptr)
        {
            qDebug() << "QTableWidgetItem null pointer at m_tableWidget->item("
                << i+pkt_offset << ",2)";
            continue;
        }
        if( (i*4+3) < regs.size())
        {
            uint32_t v1 = regs.at(4*i+0) & 0x0ff;
            uint32_t v2 = regs.at(4*i+1) & 0x0ff;
            uint32_t v3 = regs.at(4*i+2) & 0x0ff;
            uint32_t v4 = regs.at(4*i+3) & 0x0ff;

            uint32_t val = (v4 << 24)  | (v3 << 16)
                        | (v2 << 8) | (v1);
            twi->setText(QString("0x%1").arg(val,8,16,QLatin1Char('0')));

            if(val != m_lastRegs.at(i+pkt_offset))
            {
                QFont f;
                if(m_bold_changes)
                    f.setBold(true);
                else
                    f.setBold(false);
                twi->setFont(f);
                //QBrush b;
                //b.setColor(Qt::cyan);
                //twi->setForeground(b);
                m_lastRegs[i+pkt_offset] = val;
            }
            else
            {
                QFont f;
                f.setBold(false);
                twi->setFont(f);
                //QBrush b;
                //b.setColor(Qt::blue);
                //twi->setForeground(b);
            }

        }
        else
        {
            twi->setText(QString("???"));
        }
    }
    m_bold_changes = true;

    ++m_reg_blk_idx;
    if(m_reg_blk_idx >= m_reg_blks.size()) // have read all the register blocks
    {
        m_dlyTimer->start(500); // connected to "onDelayDone"
        return;
    }

    while(m_reg_blks.at(m_reg_blk_idx).is_hole)
    {
        ++m_reg_blk_idx;
        if(m_reg_blk_idx >= m_reg_blks.size())
        {
            m_dlyTimer->start(500); // connected to "onDelayDone"
            return;
        }
    }

    m_update_ch->rw(m_reg_blks.at(m_reg_blk_idx).base
                    , m_reg_blks.at(m_reg_blk_idx).len
                    ,nullptr, Gem_rw_type::READ_INC);

    //m_dlyTimer->start(500); // connected to "onDelayDone"
}

// Handle response to individual user writes to FPGA registers by logging any
// failure. Failure should be apparent to user from unchanged register value
// shown in GUI
void Fpga_window::onRegWriteDone(bool timeout, uint32_t base, uint32_t numRegs
                                , QByteArray regs, Gem_rw_type op
                                , bool isAck, uint8_t failCode)
{
    Q_UNUSED(numRegs);
    Q_UNUSED(regs);
    Q_UNUSED(op);
    Q_UNUSED(failCode);

    if(timeout)
    {
        qDebug() << "Timeout on register write to "
                 << QString("0x%1").arg(base,8,16,QLatin1Char('0'));
        return;
    }

    if(!isAck)
    {
        qDebug() << "FAIL RegWrite to "
                 << QString("0x%1").arg(base,8,16,QLatin1Char('0'));
    }
}

void Fpga_window::onDelayDone()
{
    updateFromFPGA2();
}

void Fpga_window::onCnxFailed()
{
    qDebug() << "Fpga_window::onCnxFailed";
        m_continueRunning = false;
    //m_textEdit->append("Fpga_window::onCnxFailed");
    QMessageBox msg;
    QString txt = QString("Connection to %1:%2 failed")
                        .arg(m_addr.toString()).arg(m_port);
    msg.setText(txt);
    msg.setIcon(QMessageBox::Critical);
    QPoint posn = this->pos();
    msg.move(posn.x()+40, posn.y()+100);
    msg.exec();

    if(m_update_ch)
        m_update_ch->dispose();
    m_update_ch = nullptr;
    this->close();
}

// Event handler for when user directly changes the base address that they
// want displayed
void Fpga_window::onBaseAddrChange()
{

    bool ok;
    uint32_t val = m_lineEdit->text().toInt(&ok, 0);
    if(ok)
    {
        m_tableBase = val;

        Reg_block rb;
        rb.base = m_tableBase;
        rb.len = m_numRegs;
        rb.is_hole = false;
        m_reg_blks.clear();
        m_reg_blks.push_back(rb);

        //print_reg_blks();
    }
}

// Event handler for when the user directly sets the number of registers
// that they want displayed
void Fpga_window::onLengthChange()
{
    bool ok;
    uint32_t val = m_lineEdit2->text().toInt(&ok, 0);
    if(ok)
    {
        m_numRegs = val;

        Reg_block rb;
        rb.base = m_tableBase;
        rb.len = m_numRegs;
        rb.is_hole = false;
        m_reg_blks.clear();
        m_reg_blks.push_back(rb);

        //print_reg_blks();
    }
}

// Event handler triggered when the item the user has in selected
// in the tree widget changes, ie when they want to look at a different
// peripheral address in the FPGA
void Fpga_window::onTreeWidgetItemSelectionChanged()
{
    QTreeWidgetItem * itm = m_treeWidget->currentItem();
    // Only update address for children ports, not top level peripheral entries
    if(itm->type() == -1)
        return;

    int port_num = itm->type();
    Axi_port_t *axi_port = m_am->get_port(port_num);
    m_tableBase = axi_port->base_addr;
    uint32_t n_regs = axi_port->n_regs;
    //if(n_regs <= 100)   // TODO fixme
        m_numRegs = n_regs;
    //else
    //    m_numRegs = 100;

    m_lineEdit->setText(QString("0x%1").arg(m_tableBase
                                            , 8, 16, QLatin1Char('0')));

    m_reg_blks = get_reg_read_list(port_num);
    //print_reg_blks();
}

void Fpga_window::adjust_address_display()
{
    for(uint32_t row=0; row<m_numRegs; row++)
    {
        // Update register name corresponding to this cell
        QTableWidgetItem * twi = m_tableWidget->item(row,0);
        auto names = m_am->peripheral_name_from_addr(m_am->all_ports()
                                                     , m_tableBase + row);
        if(names.size() != 0)
        {
            twi->setText(names.front());
            QString tip = m_am->reg_descr_from_addr(m_am->all_ports()
                                                    , m_tableBase+row);
            if(tip.size() != 0)
            {
                twi->setToolTip(tip);
            }
        }
        else
            twi->setText( QString("unknown 0x%1").arg(m_tableBase+row
                                                  ,8,16,QLatin1Char('0')));

        // Update address corresponding to this cell
        twi = m_tableWidget->item(row,1);
        twi->setText( QString("0x%1").arg(m_tableBase+row,8,16
                    ,QLatin1Char('0')));
        // Clear old value
        twi = m_tableWidget->item(row,2);
        twi->setText( QString("?"));
    }
    //m_tableBase = table_base;
}

void Fpga_window::add_remove_tablewidgets()
{
    // add or remove table widgets
    if(m_numRegs < m_lastNumRegs)
    {
        for(unsigned int row=m_lastNumRegs-1; row>=m_numRegs; row--)
        {
            m_tableWidget->removeRow(row);
        }
        m_tableWidget->setRowCount(m_numRegs);
    }
    else if(m_numRegs > m_lastNumRegs)
    {
        m_tableWidget->setRowCount(m_numRegs);
        for(unsigned int row = m_lastNumRegs; row<m_numRegs; row++)
        {
            // First column items
            QTableWidgetItem *tItem = new QTableWidgetItem();
            QList<QString> reg_names = m_am->peripheral_name_from_addr(
                        m_am->all_ports(), m_tableBase+row);
            if(reg_names.size() != 0)
            {
                tItem->setText(reg_names.front());
                QString tip = m_am->reg_descr_from_addr(m_am->all_ports()
                                                        , m_tableBase+row);
                if(tip.size() != 0)
                {
                    tItem->setToolTip(tip);
                }
            }
            else
            {
                tItem->setText( QString("unknown 0x%1").arg(m_tableBase+row
                                                        ,8,16,QLatin1Char('0')));
            }
            tItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            tItem->setFlags( Qt::NoItemFlags); // no select, edit, etc
            QBrush b;
            b.setColor(Qt::black);
            tItem->setForeground(b);
            m_tableWidget->setItem(row, 0, tItem);

            // Second column items
            tItem = new QTableWidgetItem();
            tItem->setText( QString("0x%1").arg(m_tableBase+row,8,16
                        ,QLatin1Char('0')));
            tItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            tItem->setFlags( Qt::NoItemFlags); // no select, edit, etc
            b.setColor(Qt::black);
            tItem->setForeground(b);
            m_tableWidget->setItem(row, 1, tItem);

            // Third column items
            tItem = new QTableWidgetItem();
            tItem->setText( QString("??"));
            tItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            tItem->setFlags( Qt::ItemIsEnabled); // no select, edit, but clickable
            b.setColor(Qt::blue);
            tItem->setForeground(b);
            m_tableWidget->setItem(row, 2, tItem);
        }
    }
    //m_numRegs = newNumRegs;
    m_lineEdit2->setText(QString("%1").arg(m_numRegs));
}

// Handle double click on register value (User wants to write to register)
void Fpga_window::onItemDoubleClicked(QTableWidgetItem *twi)
{
    uint32_t reg_addr = m_tableBase+twi->row();

    qDebug() << "Double click, row=" << twi->row() << " write to register "
             << QString("0x%1").arg(reg_addr,8,16,QLatin1Char('0'));

    // Pop up a dialog to ask for a new register value
    bool ok;
    QString txt = QString( "REG[0x%1]").arg(reg_addr,8,16,QLatin1Char('0'));
    QString input = QInputDialog::getText(this, txt
            , "New value:", QLineEdit::Normal, twi->text(), &ok);
    if(!ok || input.isEmpty())
        return;

    // decode the value the user typed
    uint32_t value = input.toUInt(&ok, 0); // accept base 8, 10, or 16 input
    if(!ok)
    {
        qDebug() << "Error: nrecognised input value for register: " << input;
        return;
    }

    // Write the value to the gemini FPGA register
    m_writer_ch->rw(reg_addr, 1, &value, Gem_rw_type::WR_INC);
}

void Fpga_window::onUploadClicked(bool checked)
{
    Q_UNUSED(checked);
    qDebug() << "Fpga_window::onUploadClicked";
    QString fname = QFileDialog::getOpenFileName(this
                         , "Upload Gemini File", "", "Gemini upload data (*.dat *.txt)");
    if(fname.isEmpty())
    {
        //qDebug() << "User aborted file upload";
        return;
    }

    // Open the file
    QFile upload_file(fname);
    if (!upload_file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug().noquote() << "Fpga_window::onUploadClicked() Cant open "
                           << fname;

        QMessageBox msg;
        msg.setText("Can't open file " + fname);
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("Gemini Upload error");
        msg.exec();
        return;
    }

    QString header;
    QVector<uint32_t> data;
    bool data_conversion_failure = false;
    uint32_t reg_base_addr = 0;
    uint32_t reg_offset = 0;
    while (!upload_file.atEnd())
    {
        // Read a line
        QByteArray line = upload_file.readLine();

        // Strip comment from line
        int idx = line.indexOf('#');
        if(idx >= 0)
            line.truncate(idx);

        // Scan line for a destination header
        idx = line.indexOf("[");
        if(idx >= 0)  // header with destination for all the data that follows
        {
            if(!header.isEmpty())
            {
                if(!data_conversion_failure)
                {
                    qDebug().noquote() << data.size() << "items loaded to" << header
                                       << QString("base=0x%1 offset=0x%2").arg(
                                              reg_base_addr,8,16,QLatin1Char('0')).arg(
                                              reg_offset,8,16,QLatin1Char('0'));
                    // TODO write data
                    uint32_t addr = reg_base_addr + reg_offset;
                    m_update_ch->rw(addr, data.size(), data.data(), Gem_rw_type::WR_INC);
                }
                else
                    qDebug().noquote() << "ERROR - Ignoring data for " << header;
            }
            header = line;


            QString hdr_line(line);
            bool ok = find_addr(hdr_line, &reg_base_addr, &reg_offset);
            if(!ok)
                qDebug().noquote() << "Bad header" << hdr_line;
            data_conversion_failure = !ok;
            data.clear();
            data.reserve(128);
        }
        else  // This line contains data to be programmed
        {
            if(!header.isEmpty()) // if we have a header, scan line for data
            {
                QString s = line.simplified();
                if(s.size() > 0)
                {
                    QStringList sl = s.split(QRegExp("\\s+"));
                    for(auto tok: sl)
                    {
                        bool ok;
                        uint32_t val = tok.toUInt(&ok, 0);
                        if(ok)
                        {
                            //qDebug() << QString("<- 0x%1").arg(val,8,16,QLatin1Char('0'));
                            data.append(val);
                        }
                        else
                        {
                            qDebug() << QString("xx 0x%1").arg(val,8,16,QLatin1Char('0'));
                            data_conversion_failure = true;
                        }
                    }
                }

            }
            // else discard data (before first header line)
        }


    }
    if(data.size() != 0)
    {
        if(!data_conversion_failure)
        {
            qDebug().noquote() << data.size() << "regs about to load to" << header;
            // TODO write data
            uint32_t addr = reg_base_addr + reg_offset;
            m_update_ch->rw(addr, data.size(), data.data(), Gem_rw_type::WR_INC);
            qDebug().noquote() << data.size() << "regs loaded to" << header;
        }
        else
        {
            qDebug().noquote() << "ERROR - Ignoring data for " << header;
        }

    }
    upload_file.close();

}

// The hdrline string should be a string like "[periph.group.field] [offset]"
// We look up address map for "periph.group.field" base address. If it fails
// return false, otherwise return true.
// If an offset is present and valid, we return it, but if not we return zero
// for the offset value.
bool Fpga_window::find_addr(QString &hdrline, uint32_t *base, uint32_t *offset)
{
    int idx = hdrline.indexOf("[");
    if(idx < 0)
        return false;

    int idx2 = hdrline.indexOf("]", idx);
    if(idx2 < 0)
        return false;

    QString addr_name = hdrline.mid(idx+1,idx2-idx-1);
    QStringList addr_parts = addr_name.split('.');
    if(addr_parts.size() != 3)
        return false;

    bool ok;
    uint32_t reg_base_addr = m_am->get_base_addr( addr_parts[0]
            , addr_parts[1], addr_parts[2], &ok);
    if(!ok)
        return false;
    *base = reg_base_addr;
    *offset = 0;

    // Decode optional offset if present
    int idx3 = hdrline.indexOf('[', idx2);
    if(idx3 < 0)
        return true;

    int idx4 = hdrline.indexOf(']', idx3);
    if(idx4 < 0)
        return true;

    QString offst = hdrline.mid(idx3+1,idx4-idx3-1);
    uint32_t off = offst.toInt(&ok, 0);
    if(ok)
        *offset = off;

    return true;
}

void Fpga_window::onDownloadClicked(bool checked)
{
    Q_UNUSED(checked);
    qDebug() << "Fpga_window::onDownloadClicked";
    Download_dialog * dd = new Download_dialog();
    int rv = dd->exec();
    if(rv != QDialog::Accepted)
    {
        qDebug() << "Download dialog cancelled by user";
        dd->hide();
        dd->deleteLater();
        return;
    }
        qDebug() << "Download dialog completed by user";

    QString reg_name = dd->get_reg_name();
    uint32_t reg_offset = dd->get_reg_offset();
    uint32_t length = dd->get_length();
    QString filename = dd->get_file_name();
    bool valid = dd->is_valid();

    dd->hide();
    dd->deleteLater();

    if(!valid)
    {
        QMessageBox msg;
        msg.setText("Invalid entries ");
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("Register entry error");
        msg.exec();

        return;
    }

    QString bracketed_name = QString("[") + reg_name + QString("]");
    uint32_t base = 0;
    uint32_t unused_offset = 0;
    bool ok = find_addr(bracketed_name, &base, &unused_offset);

    if(!ok)
    {
        QMessageBox msg;
        msg.setText("Invalid register: " + reg_name);
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("Register name error");
        msg.exec();

        return;
    }

    qDebug().noquote() << QString("Download from 0x%1, length 0x%2, to %3")
                          .arg(base+reg_offset,8,16,QLatin1Char('0'))
                          .arg(length,8,16,QLatin1Char('0'))
                          .arg(filename);

    // Put header in download file
    m_dl_filename = filename;
    QFile dl_file(m_dl_filename);
    if(!dl_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        QMessageBox msg;
        msg.setText("Can't open file:" + m_dl_filename);
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("File error");
        msg.exec();
        qDebug() << "Can't open download output file:" << m_dl_filename;
        return;
    }
    QTextStream out(&dl_file);
    QString txt = QString("%1 [%2]  # -- download from 0x%3 length 0x%4 --\n")
                        .arg(bracketed_name)
                        .arg(reg_offset,0,16)
                        .arg(base+reg_offset, 8,16,QLatin1Char('0'))
                        .arg(length, 0,16);
    out << txt;
    dl_file.close();

    m_download_ch->rw(base+reg_offset, length, nullptr, Gem_rw_type::READ_INC);

    return;
}

void Fpga_window::onDownloadResult(bool timeout, uint32_t base
            , uint32_t numRegs, QByteArray regs, Gem_rw_type op
            , bool isAck, uint8_t failCode)
{
    Q_UNUSED(timeout);
    Q_UNUSED(regs);
    Q_UNUSED(op);
    Q_UNUSED(failCode);
    if(timeout)
    {
        qDebug() << "download timeout";
        return;
    }
    if(!isAck)
    {
        qDebug() << "Download NACK";
        return;
    }

    qDebug() << QString("Got download base 0x%1, len 0x%2)")
                .arg(base,8,16,QLatin1Char('0')).arg(numRegs,8,16);
    QFile dl_file(m_dl_filename);
    if(!dl_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        qDebug() << "Can't write download data to file" << m_dl_filename;
        return;
    }

    QTextStream out(&dl_file);

    out << QString("# base 0x%1, length 0x%2\n")
                  .arg(base,8,16,QLatin1Char('0'))
                  .arg(numRegs);

    uint32_t *regdata = reinterpret_cast<uint32_t *>(regs.data());
    for(unsigned int i = 0; i<numRegs; i++)
    {
        if((base+i)%16 == 0)
        {
            out << QString("0x%1").arg(regdata[i],8,16,QLatin1Char('0'))
                << QString("    # [0x%1]\n").arg(base+i,8,16,QLatin1Char('0'));
        }
        else
        {
            out << QString("0x%1\n").arg(regdata[i],8,16,QLatin1Char('0'));
        }
    }
    dl_file.close();

}

// Create a vector of contiguous address blocks in the port address map
// distinguishing between registers and holes (gaps between registers)
std::vector<Reg_block> Fpga_window::get_reg_read_list(uint32_t idx)
{
    std::vector<Reg_block> blocks;
    blocks.reserve(10);

    bool is_first = true;
    uint32_t base = 0;
    uint32_t len = 0;
    uint32_t next = 0;

    auto port = m_am->get_port(idx);
    for(auto field: port->fields)
    {
        if(is_first)
        {
            is_first = false;
            if(port->base_addr != field.base_addr) // hole at start
            {
                Reg_block hole;
                hole.base = port->base_addr;
                hole.len = field.base_addr - port->base_addr;
                hole.is_hole = true;
                blocks.push_back(hole);
            }

            base = field.base_addr;
            len = field.n_regs;
            next = field.n_regs + base;
        }
        else
        {
            if(field.base_addr < next) // another bitfield in same register
            {
                continue;
            }
            else if (field.base_addr == next) // next contiguous register
            {
                len += field.n_regs;
                next +=field.n_regs;
            }
            else // this register comes after a hole in the address map
            {
                Reg_block rb;
                rb.base = base;
                rb.len = len;
                rb.is_hole = false;
                blocks.push_back(rb);

                Reg_block hole;
                hole.base = next;
                hole.len = field.base_addr - next;
                hole.is_hole = true;
                blocks.push_back(hole);

                base = field.base_addr;
                len = field.n_regs;
                next = field.n_regs + base;
            }
        }
    }

    if(len != 0)
    {
        Reg_block rb;
        rb.base = base;
        rb.len = len;
        rb.is_hole = false;
        blocks.push_back(rb);
    }
    return blocks;
}

void Fpga_window::print_reg_blks()
{
    qDebug() << "Register Read List";
    for(unsigned int i=0; i< m_reg_blks.size(); i++)
    {
        QString txt = QString("  Reg base 0x%1 len 0x%2")
                .arg(m_reg_blks.at(i).base,0,16)
                .arg(m_reg_blks.at(i).len,0,16);
        if(m_reg_blks.at(i).is_hole)
            txt.append(" (hole)");
        qDebug().noquote() << txt;
    }
}
