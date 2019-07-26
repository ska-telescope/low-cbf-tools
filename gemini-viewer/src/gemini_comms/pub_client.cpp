#include "pub_client.h"
#include <QtEndian>

// on-wire order of packet contents
struct Gemini_publish_packet
{
        uint8_t ver;
        uint8_t cmd;
        uint16_t reserved;
        uint32_t event;
        uint32_t time_ls;
        uint32_t time_ms;
};

Pub_client::Pub_client(uint16_t listen_port_num)
{
    m_soc = new QUdpSocket(this);
    m_soc->bind(QHostAddress::AnyIPv4, listen_port_num
            , QAbstractSocket::ReuseAddressHint);
    connect(m_soc, SIGNAL(readyRead()), this, SLOT(on_pkt_ready_read()));
}

Pub_client::~Pub_client()
{
    disconnect(m_soc, SIGNAL(readyRead()), this, SLOT(on_pkt_ready_read()));
}

void Pub_client::on_pkt_ready_read()
{
    while(m_soc->hasPendingDatagrams())
    {
        QHostAddress addr;
        uint16_t port;
        Gemini_publish_packet pkt;
        char *pktptr = reinterpret_cast<char*>(&pkt);
        int64_t len = m_soc->readDatagram(pktptr, sizeof(pkt), &addr, &port);

        // ignore everything that isn't right length for Gemini Publish pkt
        if(len != sizeof(Gemini_publish_packet))
            continue;
        // ignore everything that isn't right gemini version
        if(pkt.ver != 1)
            continue;

        // OK notify subscribers about anything else
        notify_subscribers(pkt, addr, port);
    }
}

void Pub_client::notify_subscribers(Gemini_publish_packet &pkt
        , QHostAddress & sender_addr, uint16_t sender_port)
{
    Pub_event ev;
    ev.from_ip = sender_addr;
    ev.from_port = sender_port;
    // Convert multi-byte data from network-byte-order (big-endian)
    ev.event = qFromBigEndian(pkt.event);
    ev.cmd = pkt.cmd;
    ev.timestamp = qFromBigEndian(pkt.time_ms);
    ev.timestamp = (ev.timestamp<<32) | qFromBigEndian(pkt.time_ls);

    emit ready_read(ev);
}
