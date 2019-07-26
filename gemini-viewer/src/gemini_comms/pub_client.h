/* Class that handles Gemini Publish packets received from the network
 *   - listens for all Gemini packets to the (broadcast) gemini publish port
 *   - calls back to subscribers that have registered interest in particular
 *       source IPs and/or source ports, and/or particular events
 */
#ifndef PUB_CLI_H
#define PUB_CLI_H

#include <cstdint>
#include <functional>
#include <list>
#include <QUdpSocket>

#define DEFAULT_PUBLISH_PORT 30001

class Subscriber;
struct Gemini_publish_packet;

struct Pub_event
{
    QHostAddress from_ip;
    uint16_t from_port;
    uint8_t cmd;
    uint32_t event;
    uint64_t timestamp;
};

class Pub_client: public QObject
{
    Q_OBJECT

    private:
        QUdpSocket *m_soc;

        void notify_subscribers(Gemini_publish_packet & pkt
                    , QHostAddress & sender_addr, uint16_t sender_port);
    private slots:
        void on_pkt_ready_read();
    public:
        Pub_client(uint16_t listen_port_num = DEFAULT_PUBLISH_PORT);
        ~Pub_client();
        Pub_client(const Pub_client &) = delete; // no copy
        Pub_client& operator=(Pub_client &) = delete; // no assign
    signals:
        void ready_read(Pub_event ev);
};

#endif
