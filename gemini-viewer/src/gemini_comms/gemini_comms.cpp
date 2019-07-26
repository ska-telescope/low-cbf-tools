#include "gemini_comms.h"
#include <QtEndian>
#include <QDateTime>
#include <QDebug>

#define GEMVER 1
#define MIN_GEMCNX_BYTES 12

#define PKT_OVERHEAD_BYTES 54 // Ethernet+IP+UDP+Gemini header total size

//enum class CommState { NOT_CONNECTED, REJECTED, CONNECTED, FAILED };

enum class GemCmd:uint8_t {CNX=1, READ_R, READ_I, WRITE_R, WRITE_I
                           , ACK=0x10, NACKT=0x20, NACKP=0x40, PUB=0x80};

// Trace data for received packets
#define RECORD_LEN 8  // number of recent packets to keep as debug trace
#define RECORD_NBYTES 64 // max number of bytes from each packet for debug
struct Packet_record
{
    uint32_t len;
    uint8_t data[RECORD_NBYTES];
};

static QString toCmdName(GemCmd cmd)
{
    switch(cmd)
    {
    case GemCmd::CNX:
        return QString("CNX");
    case GemCmd::READ_R:
        return QString("READ_FIFO");
    case GemCmd::READ_I:
        return QString("READ");
    case GemCmd::WRITE_R:
        return QString("WRITE_FIFO");
    case GemCmd::WRITE_I:
        return QString("WRITE");
    case GemCmd::ACK:
        return QString("ACK");
    case GemCmd::NACKT:
        return QString("NACKT");
    case GemCmd::NACKP:
        return QString("NACKP");
    case GemCmd::PUB:
        return QString("PUBLISH");
    default:
        return QString("UNKNOWN_CMD");
    }
}

// on-wire order of Gemini packet header
struct Gemini_comms_hdr
{
        uint8_t ver;
        GemCmd op;
        uint8_t cli_seq;
        uint8_t svr_seq;
        uint32_t base_addr;
        uint16_t num_regs;
        uint16_t fail_code;
};
// on-wire order of Gemini connect response payload
struct Gemini_payload_cnx_ack
{
    uint32_t maxpdu;
    uint32_t pipeline;
    uint32_t cnxid;
};

// Record of transaction with remote card
struct Gemini_transaction
{
    Gem_rw_type rwType;
    uint32_t base;
    uint32_t numRegs;
    uint32_t sendCount;
    qint64 timeoutTimeMs;
    uint8_t cli_seq;
    uint32_t ctx;
    QByteArray data;
};



Gemini_comms::Gemini_comms(QHostAddress addr, const uint16_t port
                , const uint64_t timeout_msec
                , const uint32_t retries)
    : m_addr(addr)
    , m_port(port)
    , m_timeoutmsec(timeout_msec)
    , m_maxRetries(retries)
    , m_isConnected(false)
    , m_numInTransit(0)
    , m_maxPduBytes(128) // initially only small packets sent
{
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_skt = new QUdpSocket(this);

    // Reserve some space so re-allocation doesn't have to be done often
    // Clients might have say 5 channels they use
    m_chans.reserve(5);
    qDebug() << "Gemini_comms::Gemini_comms vector capacity"
             << m_chans.capacity();

}

Gemini_comms::~Gemini_comms()
{
    m_timer->stop();
    m_timer->deleteLater();
    // TODO not sure what will need to be disconnected. Can skip bec automatic?
    //disconnect(m_skt, &QUdpSocket::readyRead, this, &Gemini_comms::onRxReady);

    // client should have called dispose on its channels
    for(auto it=m_chans.begin(); it!=m_chans.end(); ++it)
    {
        auto chan = *it;
        if(chan)
        {
            qWarning() << "~Gemini_comms: Deleting channel that client forgot"
                     << "ctx =" << chan->m_ctx;
            chan->deleteLater();
            *it = nullptr;
        }
    }
    m_skt->deleteLater();

    // Clean out received packet trace
    while( m_received_trace.length() > 0)
    {
        auto pkt_rec = m_received_trace.front();
        m_received_trace.pop_front();
        delete pkt_rec;
    }
}

// Low-level packet write
void Gemini_comms::sendPdu(char *data, uint32_t len)
{
    if(len < m_maxPduBytes)
        m_skt->writeDatagram(data, len, m_addr, m_port);
    else
        qCritical().noquote() << "Discard oversize PDU to" << m_addr.toString()
                           << ":" << m_port << "(len =" << len << "bytes)";
}

/* ================  Methods for connecting to FPGA server ================= */
// Initiate connection to Gemini server (FPGA)
void Gemini_comms::udpConnect()
{
    if(m_isConnected)
    {
        emit cnx_result(Gem_cnx_rslt::OK);
        return;
    }

    m_cnxRetryCount = 0;
    m_cli_seq = 0x1;
    connect(m_skt, &QUdpSocket::readyRead
                    , this, &Gemini_comms::onCnxRxReady);
    connect(m_timer, &QTimer::timeout
                    , this, &Gemini_comms::onCnxTimeout);
    onCnxTimeout();
}
// Sends both first Connect attempt and subsequent retries
void Gemini_comms::onCnxTimeout()
{

    if(m_cnxRetryCount >= m_maxRetries)
    {
        disconnect(m_timer, &QTimer::timeout
                   , this, &Gemini_comms::onCnxTimeout);
        disconnect(m_skt, &QUdpSocket::readyRead
                   , this, &Gemini_comms::onCnxRxReady);
        qDebug().nospace().noquote() << "Timeout on Connect to "
                                     << m_addr.toString() << ":" << m_port
                                     << " (" << m_cnxRetryCount << " tries)";
        emit cnx_result(Gem_cnx_rslt::TIMEOUT);
        return;
    }
    ++m_cnxRetryCount;
    m_timer->start(m_timeoutmsec);
    Gemini_comms_hdr pkt = { GEMVER, GemCmd::CNX, m_cli_seq
                             , 0x0, 0x0, 0x0, 0x0};
    sendPdu(reinterpret_cast<char *>(&pkt), sizeof(pkt));
}
// Reads all packets while expecting a CNX response
void Gemini_comms::onCnxRxReady()
{
    qint64 len;
    char buf[sizeof(Gemini_comms_hdr) + sizeof(Gemini_payload_cnx_ack)+8];
    Gemini_comms_hdr *pkt = reinterpret_cast<Gemini_comms_hdr *>(buf);
    len = m_skt->readDatagram(buf, sizeof(buf));

    // Is packet too small to be a Gemini connection response
    if(len < static_cast<int64_t>(sizeof(Gemini_comms_hdr)
                                + sizeof(Gemini_payload_cnx_ack)))
    {
        qWarning() << "CNX packet RX" << len << "bytes";
        //return; //FIXME: uncomment when software simulation is fixed < ----------------
    }

    // Is the packet wrong version, wrong number of regs for CNX ACK/NACK
    if((pkt->ver != GEMVER)
            // FTODO: should server_seq always be 1, not just when ACK received?
            || ((pkt->op == GemCmd::ACK) && (pkt->svr_seq != 1))
            // FIXME: uncomment when software simulation is fixed  <---------------------
            // || (qFromBigEndian(pkt->num_regs) != 3)
            )
    {
        qWarning().nospace() << "CNX packet RX, got ver=" << pkt->ver
                           << " cmd=" << static_cast<uint8_t>(pkt->op)
                           << " svr_seq=" << pkt->svr_seq
                           << " cli_seq=" << pkt->cli_seq
                           << " num_regs=" << qFromBigEndian(pkt->num_regs);
        return;
    }

    switch(pkt->op)
    {
    case GemCmd::ACK:
        m_timer->stop();
        disconnect(m_timer, &QTimer::timeout
                   , this, &Gemini_comms::onCnxTimeout);
        disconnect(m_skt, &QUdpSocket::readyRead
                   , this, &Gemini_comms::onCnxRxReady);

        {
        Gemini_payload_cnx_ack *ack_data =
                reinterpret_cast<Gemini_payload_cnx_ack*>(pkt+1);
        m_maxPayloadWords = qFromLittleEndian(ack_data->maxpdu);
        // FPGAs used to advertise 1990 words payload, but really only handled
        //   1985 words before ethernet MAC failed. Use 1984 if we detect this.
        if(m_maxPayloadWords == 1990)
        {
            qDebug() << "FPGA workaround, using 1984 words instead of 1990"
                     << " for maximum register payload length";
            m_maxPayloadWords = 1984;
        }
        m_maxPduBytes = m_maxPayloadWords*4; // words->bytes
        m_pipelineLen = qFromLittleEndian(ack_data->pipeline);

        qDebug().nospace() << "Got CNX ACK ver=" << pkt->ver
                 << " cmd=" << static_cast<uint8_t>(pkt->op)
                 << " svr_seq=" << pkt->svr_seq
                 << " cli_seq=" << pkt->cli_seq
                 << " num_regs=" << qFromLittleEndian(pkt->num_regs)
                 << " max_pdu=" << m_maxPayloadWords
                 << " pipeline=" << m_pipelineLen
                 << " cnxid=" << qFromLittleEndian(ack_data->cnxid)
                 << " len=" << len;

        }
        m_isConnected = true;
        m_cli_seq = pkt->svr_seq;
        m_numInTransit = 0;
        m_waitingForRetryResponse = false;
        connect(m_skt, &QUdpSocket::readyRead
                   , this, &Gemini_comms::onPktRxReady);
        connect(m_timer, &QTimer::timeout
                , this, &Gemini_comms::onPktTimeout);

        emit cnx_result(Gem_cnx_rslt::OK);
        break;

    case GemCmd::NACKT:
        m_timer->stop();
        disconnect(m_timer, &QTimer::timeout
                   , this, &Gemini_comms::onCnxTimeout);
        disconnect(m_skt, &QUdpSocket::readyRead
                   , this, &Gemini_comms::onCnxRxReady);
        qDebug().nospace() << "Got CNX NACKT ver=" << pkt->ver
                 << " cmd=" << static_cast<uint8_t>(pkt->op)
                 << " svr_seq=" << pkt->svr_seq
                 << " cli_seq=" << pkt->cli_seq
                 << " len=" << len;
        emit cnx_result(Gem_cnx_rslt::FAIL_TEMP);
        break;

    case GemCmd::NACKP:
        m_timer->stop();
        disconnect(m_timer, &QTimer::timeout
                   , this, &Gemini_comms::onCnxTimeout);
        disconnect(m_skt, &QUdpSocket::readyRead
                   , this, &Gemini_comms::onCnxRxReady);
        qDebug().nospace() << "Got CNX NACKP ver=" << pkt->ver
                 << " cmd=" << static_cast<uint8_t>(pkt->op)
                 << " svr_seq=" << pkt->svr_seq
                 << " cli_seq=" << pkt->cli_seq
                 << " len=" << len;
        emit cnx_result(Gem_cnx_rslt::FAIL_PERM);
        break;

    default:
        qDebug().nospace() << "Unexpected CNX response ver=" << pkt->ver
                 << " cmd=" << static_cast<uint8_t>(pkt->op)
                 << " svr_seq=" << pkt->svr_seq
                 << " cli_seq=" << pkt->cli_seq
                 << " len=" << len;
        // Wait to see if another packet arrives or timeout
        break;
    }
}
/* ===========  End of Methods for connecting to FPGA server ============== */

// Method to read/write remote FPGA registers accessed by RwChannel objects
void Gemini_comms::rw(uint32_t base, uint32_t numRegs, uint32_t *regs
                      , Gem_rw_type opType, uint32_t ctx)
{
    if(!m_isConnected)
    {
        qWarning() << "Attempt to send when not connected";
        return;
    }

    //if(numRegs > m_maxPayloadWords)
    //    qDebug() << "Long transaction" << numRegs << "registers (regs@" << regs << ")";

    // Split write into transactions that don't exceed maximum PDU size
    while(numRegs > 0)
    {
        uint32_t regs_sent = numRegs;
        if(regs_sent > m_maxPayloadWords)
            regs_sent = m_maxPayloadWords;

        Gemini_transaction *t = new Gemini_transaction;
        t->rwType = opType;
        t->base = base;
        t->numRegs = regs_sent;
        if((opType == Gem_rw_type::WR_INC) || (opType == Gem_rw_type::WR_FIFO))
        {
            QByteArray dataBytes(reinterpret_cast<char *>(regs), regs_sent*4);
            t->data = dataBytes;
        }
        else
        {
            t->data = nullptr;
        }
        t->sendCount = 0;
        t->cli_seq = 0;
        t->ctx = ctx;

        m_transList.push_back(t);
        //qDebug() << "added new transaction. (" << regs_sent << "registers) ["
        //         << m_transList.size() << "waiting." << m_numInTransit << "in transit]";
        numRegs -= regs_sent;
        regs += regs_sent;
        base += regs_sent;

    }

    if(m_waitingForRetryResponse)
        qDebug() << "   .. waitingForRetry";

    trySendTransactions();
}


// Send more transactions if possible
void Gemini_comms::trySendTransactions()
{
    // Don't send any more if we're waiting for a retry response
    if(m_waitingForRetryResponse)
        return;

    //qDebug() << "Gemini_comms::trySendTransactions m_transList.size()=" << m_transList.size();
    // Go through the transactions in order of submission
    for(auto it = m_transList.begin(); it!=m_transList.end(); ++it)
    {
        // Don't send beyond the maximum the FPGA can buffer
        if(m_numInTransit >= m_pipelineLen)
            break;

        // Skip already-sent transactions (reply in transit)
        auto trans = *it;
        if(trans->sendCount != 0)
            continue;

        sendGeminiTransaction(trans);
    }
}

void Gemini_comms::resendOldestTransaction()
{
    Gemini_transaction *oldestTrans = m_transList.front();
    if(oldestTrans->sendCount > m_maxRetries)
    {
        // Exceeded the number of retries: connection failed
        qDebug().noquote().nospace() << "Connection to "
                                     << m_addr.toString() << ":"
                                     << m_port << " FAILED on all "
                                     << m_maxRetries << " retries";
        closeConnection();
        emit cnx_failed();
        return;
    }
    // We retried, so we will have to resend all subsequent packets
    for(auto trans: m_transList)
    {
        if(trans != oldestTrans)
            trans->sendCount = 0;
    }
    // Send Retry, and prevent further sends till retry reply received
    m_cli_seq = oldestTrans->cli_seq - 1; // resend with failed seq number
    sendGeminiTransaction(oldestTrans);
    m_waitingForRetryResponse = true;
}

void Gemini_comms::sendGeminiTransaction(Gemini_transaction *trans)
{
    ++m_cli_seq;
    // Build Gemini Header
    Gemini_comms_hdr hdr = { GEMVER, GemCmd::CNX, m_cli_seq
                             , 0x0, 0x0, 0x0, 0x0};
    switch(trans->rwType)
    {
    case Gem_rw_type::READ_INC:
        hdr.op = GemCmd::READ_I;
        break;
    case Gem_rw_type::READ_FIFO:
        hdr.op = GemCmd::READ_R;
        break;
    case Gem_rw_type::WR_INC:
        hdr.op = GemCmd::WRITE_I;
        break;
    case Gem_rw_type::WR_FIFO:
        hdr.op = GemCmd::WRITE_R;
        break;
    default:
        qWarning() << "Unknown Gemini_rw_type ... skipped";
        return;
    }
    hdr.num_regs = trans->numRegs;
    hdr.base_addr = trans->base;

    // Place Header and data in send buffer
    QByteArray send_buf;
    send_buf.append((char *)&hdr, sizeof(Gemini_comms_hdr));
    send_buf.append(trans->data);

    // Send as UDP packet
    m_skt->writeDatagram(send_buf, m_addr, m_port);
    //qDebug() << "Sent" << toCmdName(hdr.op) << "seq:" << hex << m_cli_seq << dec;


    // Update accounting info
    trans->sendCount++;
    trans->timeoutTimeMs = QDateTime::currentDateTime().toMSecsSinceEpoch()
            + m_timeoutmsec;
    trans->cli_seq = m_cli_seq;
    m_numInTransit++;
    if(trans == m_transList.front())
        startTimeoutTimer(trans);
}

void Gemini_comms::closeConnection()
{
    m_isConnected = false;
    for(auto trans: m_transList)
    {
        // Notify all pending transactions that timeout occurred
        if(m_chans[trans->ctx])
            m_chans[trans->ctx]->result(true, trans->base, trans->numRegs
                                    , QByteArray(), trans->rwType, false, 0);
        delete trans;

    }
    m_transList.clear();
}

void Gemini_comms::onPktRxReady()
{
    char buf[m_maxPduBytes+PKT_OVERHEAD_BYTES];
    qint64 len = m_skt->readDatagram(buf, sizeof(buf));
    // Discard if too small to have a Gemini header
    if(len < static_cast<int64_t>(sizeof(Gemini_comms_hdr)))
    {
        qWarning() << "Runt Packet RX" << len << "bytes";
        return;
    }
    Gemini_comms_hdr *pkt = reinterpret_cast<Gemini_comms_hdr *>(buf);

    recordRxPacket(buf, len);

    // Discard if wrong gemini version
    if(pkt->ver != GEMVER)
    {
        qWarning() << "Gemini protocol version mismatch (" << pkt->ver << ")";
        return;
    }


    // If we get NACKT and svr seq < expected (lost packet TO fpga)
    // or if we get ACK and svr_seq > expected (lost packet FROM fpga)
    // then we need to retry the packet we're waiting for (oldest packet)
    // unless we've already retried and are waiting for the response
    if((pkt->op == GemCmd::NACKT)
            || (pkt->svr_seq != m_transList.front()->cli_seq))
    {
        qWarning().nospace() << "Discard Gemini packet. Expected seq="
                           << m_transList.front()->cli_seq << ", got seq= "
                           << pkt->svr_seq
                           << " (" << toCmdName(pkt->op) << ")";

        // Print trace of recently received packets
        printRxTraceToLog();

        if(!m_waitingForRetryResponse)
            resendOldestTransaction();
        return;
    }

    if((pkt->op != GemCmd::ACK) && (pkt->op != GemCmd::NACKP))
    {
        qWarning().nospace() << "Discard Gemini packet. Expected ACK but got "
                             << "cmd=" << static_cast<int>(pkt->op);
        return;
    }

    // We have a response to the request (either ACK or NACKP)
    m_timer->stop();
    if(m_waitingForRetryResponse)
    {
        m_waitingForRetryResponse = false;
        // The FPGA server's buffer will be empty because we haven't sent
        // any packets after the retry
        m_numInTransit = 0;
    }
    else
    {
        m_numInTransit--;
    }

    // read transactions should have matching number of registers returned
    if(((m_transList.front()->rwType == Gem_rw_type::READ_INC)
        || (m_transList.front()->rwType == Gem_rw_type::READ_FIFO))
            && (m_transList.front()->numRegs != pkt->num_regs))
    {
        qWarning() << "Requested" << m_transList.front()->numRegs
                   << "regs, but got reply with" << pkt->num_regs;
        printRxTraceToLog();
    }

    // Retire the transaction from the queue because it's done
    Gemini_transaction* trans = m_transList.front();
    m_transList.pop_front();
    // If the new front transaction has already been sent, then we need
    // to start its timeout timer
    if((m_transList.size() > 0) && (m_transList.front()->sendCount > 0))
    {
        startTimeoutTimer(m_transList.front());
    }
    // See if we can send additional transactions
    trySendTransactions();

    // If the requesting RxChannel was deleted while the transaction happened
    // then nobody cares about the result
    if(!m_chans[trans->ctx])
    {
        delete trans;
        return;
    }
    // Notify that there is a response
    // response is in 'buf'
    // original transaction is in 'trans'
    if((trans->rwType == Gem_rw_type::READ_FIFO)
            || (trans->rwType == Gem_rw_type::READ_INC))
    {
        QByteArray ba( buf+sizeof(Gemini_comms_hdr)
                      , len-sizeof(Gemini_comms_hdr));

        m_chans[trans->ctx]->onRwDone(false, trans->base
                            , trans->numRegs, ba, trans->rwType
                            , (pkt->op == GemCmd::ACK)
                            , pkt->fail_code);
    }
    else
    {
        QByteArray ba;
        m_chans[trans->ctx]->onRwDone(false, trans->base
                            , trans->numRegs, ba, trans->rwType
                            , (pkt->op == GemCmd::ACK)
                            , pkt->fail_code);
    }

    delete trans;
}

// Keep a list of the last received packets as a debug trace
void Gemini_comms::recordRxPacket(char buf[], qint64 len)
{
    //qDebug() << "Gemini_comms::recordRxPacket len=" << len << "reclen=" << m_received_trace.length();
    if( m_received_trace.length() >= RECORD_LEN)
    {
        auto oldest = m_received_trace.front();
        m_received_trace.pop_front();
        delete oldest;
    }
    Packet_record * rec = new Packet_record;
    rec->len = len;
    uint32_t cpy_len = len;
    if(cpy_len > RECORD_NBYTES)
        cpy_len = RECORD_NBYTES;
    memcpy(rec->data, buf, cpy_len);
    m_received_trace.push_back(rec);
    //qDebug() << "Gemini_comms::recordRxPacket done";
}

void Gemini_comms::printRxTraceToLog()
{
    while(m_received_trace.length() > 0)
    {
        auto pkt_rec = m_received_trace.front();
        m_received_trace.pop_front();

        qWarning() << "Packet trace";
        int cnt = pkt_rec->len;
        QString txt;
        for( int i=0; i<cnt; i++)
        {
            txt = txt + QString("0x%1 ").arg(pkt_rec->data[i],2,16,QLatin1Char('0'));
            if((i % 16) == 15)
            {
                qWarning().noquote() << txt;
                txt = QString("");
            }
        }
        if(txt.length() != 0)
            qWarning().noquote() << txt;
        qWarning() << "    -----";
        delete pkt_rec;
    }

}

void Gemini_comms::startTimeoutTimer(Gemini_transaction *trans)
{
    qint64 now = QDateTime::currentDateTime().toMSecsSinceEpoch();
    if(trans->timeoutTimeMs < now)
    {
        onPktTimeout();
        return;
    }

    qint64 delayms = trans->timeoutTimeMs - now;
    m_timer->start(delayms);
}

void Gemini_comms::onPktTimeout()
{
    resendOldestTransaction();
}

RwChannel * Gemini_comms::openChannel()
{
    // Find an index in the m_chans vector to use for this one
    // but check for unused slots in the vector from previous closes
    // to avoid having the vector grow too big
    uint32_t idx = 0;
    for(auto it=m_chans.begin(); it!=m_chans.end(); ++it)
    {
        RwChannel *cpt = *it;
        if(cpt == nullptr)
        {
            // If this index is still in the transaction list
            //   we don't want to re-use it just yet.
            bool isIdxInTransList = false;
            for(auto trans: m_transList)
            {
                if(trans->ctx == idx)
                {
                    isIdxInTransList = true;
                    break;
                }
            }
            if(isIdxInTransList)
            {
                idx++;
                continue;
            }

            break;
        }
        idx++;
    }
    RwChannel * chan = new RwChannel(this, idx);
    if(idx < static_cast<uint32_t>(m_chans.size()))
        m_chans[idx] = chan; // re-use an old slot
    else
        m_chans.append(chan); // add a new slot

    qDebug().nospace() << "Gemini_comms::openChannel, ctx=" << idx
             <<  " (size=" << m_chans.size() << ")";
    return chan;
}

//-----------------------

RwChannel::RwChannel(Gemini_comms *comms, uint32_t ctx)
    : m_gemComms(comms)
    , m_ctx(ctx)
{
}
RwChannel::~RwChannel()
{
    qDebug() << "RwChannel::~RwChannel()";
}

void RwChannel::dispose()
{
    m_gemComms->m_chans[m_ctx] = nullptr;
    this->deleteLater();
    qDebug() << "RwChannel::dispose() ctx=" << m_ctx << "marked for deletion";
}

void RwChannel::rw(uint32_t base, uint32_t numRegs, uint32_t *regs
                   , Gem_rw_type opType)
{
    if(!m_gemComms->m_isConnected)
    {
        emit result(true, base, numRegs, QByteArray(), opType, false, 0);
        return;
    }
    m_gemComms->rw(base, numRegs, regs, opType, m_ctx);
}

void RwChannel::onRwDone(bool timeout, uint32_t base, uint32_t numregs
                           , QByteArray regs, Gem_rw_type op, bool isAck, uint8_t fail_code)
{
    emit result(timeout, base, numregs, regs, op, isAck, fail_code);
}
