#ifndef EVENT_MODEL_H
#define EVENT_MODEL_H
#include <QStandardItemModel>
#include <vector>
#include "pub_client.h"

class Gemini_pub_info;

class Event_model: public QStandardItemModel
{
    Q_OBJECT

    private:
        // List of all the gemini cards we know about
        std::vector<Gemini_pub_info *> m_pub_info;
    public:
        Event_model(QObject *parent);
        QVariant data(const QModelIndex& index, int role) const override;
        bool reset(int row, int col);
        QHostAddress getHost(int row);
        uint16_t getPort(int row);
    public slots:
        // Callback invoked from UDP events receiver
        void recv_data(Pub_event ev);
    signals:
};

#endif
