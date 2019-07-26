#include "event_model.h"
#include <QHostAddress>
#include <QStandardItem>
#include <QList>

#include <QDebug>

class Gemini_pub_info
{
    private:
    public:
        QHostAddress from_ip;
        uint16_t from_port;
        uint32_t event_mask;
        uint32_t count;
        uint64_t timestamp;
};

Event_model::Event_model(QObject *parent)
    :QStandardItemModel(parent)
{
    QStringList hdr = {"IP addr", "Port", "Count", "Events"};
    setHorizontalHeaderLabels(hdr);
}


void Event_model::recv_data(Pub_event ev)
{
    // Is this event from a source we already know??
    int row = 0;
    for(auto idx: m_pub_info)
    {
        if((idx->from_ip == ev.from_ip) && (idx->from_port == ev.from_port))
        {
            idx->count++;
            idx->event_mask |= ev.event;
            idx->timestamp = ev.timestamp;
            // Update on-screen display
            QStandardItem *itm = new QStandardItem(QString("%1")
                                .arg(idx->count));
            setItem(row,2,itm);
            QStandardItem *itm2 = new QStandardItem(QString("0x%1")
                                .arg(idx->event_mask, 8,16,QLatin1Char('0')));
            setItem(row,3,itm2);
            return;
        }
        row++;
    }

    qDebug().nospace().noquote() << "New Gemini event source "
                                    << ev.from_ip.toString()
                                    << ":" << ev.from_port;

    // Create record of new event source
    Gemini_pub_info *inf = new Gemini_pub_info;
    inf->from_ip = ev.from_ip;
    inf->from_port = ev.from_port;
    inf->count = 1;
    inf->event_mask = ev.event;
    inf->timestamp = ev.timestamp;
    m_pub_info.push_back(inf);

    // Add new source
    QList<QStandardItem*> sil;
    // Display new row of data about the source
    QStandardItem *si = new QStandardItem( QString("%1")
                                .arg(inf->from_ip.toString()));
    si->setEditable(false);
    sil << si;
    QStandardItem *si2 = new QStandardItem( QString("%1")
                                .arg(inf->from_port));
    si2->setEditable(false);
    sil << si2;
    QStandardItem *si3 = new QStandardItem( QString("%1")
                                .arg(inf->count));
    si3->setEditable(false);
    sil << si3;
    QStandardItem *si4 = new QStandardItem( QString("0x%1")
                                .arg(inf->event_mask,8,16,QLatin1Char('0')));
    si4->setEditable(false);
    sil << si4;
    appendRow(sil);
}

// Override some of the roles (alignment of data)
QVariant Event_model::data(const QModelIndex& index, int role) const
{
    switch(role)
    {
        case Qt::TextAlignmentRole:
            return Qt::AlignCenter;
        case Qt::ToolTipRole:
            if(index.column()< 2)
                return QString("Click to open FPGA");
            else
                return QString("Click to reset");
        default: // all other roles use the base-class implementation
            return QStandardItemModel::data(index, role);
    }
}

bool Event_model::reset(int row, int col)
{
    if((row < 0) || (static_cast<unsigned>(row) >= m_pub_info.size()))
        return true;
    if(col >=4)
        return true;

    Gemini_pub_info *inf = m_pub_info[row];
    if(col == 2) // reset packet count in column 2
    {
        inf->count = 0;
        QStandardItem *itm = new QStandardItem(QString("%1")
                .arg(inf->count));
        setItem(row,2,itm);
        return true;
    }
    if(col == 3) // clear event mask in column 3
    {
        inf->event_mask = 0;
        QStandardItem *itm = new QStandardItem(QString("0x%1")
                .arg(inf->event_mask,8,16,QLatin1Char('0')));
        setItem(row,3,itm);
        return true;
    }
    return false;
}

QHostAddress Event_model::getHost(int row)
{
    if((row < 0) || (static_cast<unsigned>(row) >= m_pub_info.size()))
        return QHostAddress::Null;
    Gemini_pub_info *inf = m_pub_info[row];
    return inf->from_ip;
}

uint16_t Event_model::getPort(int row)
{
    if((row < 0) || (static_cast<unsigned>(row) >= m_pub_info.size()))
        return 0;
    Gemini_pub_info *inf = m_pub_info[row];
    return inf->from_port;
}
