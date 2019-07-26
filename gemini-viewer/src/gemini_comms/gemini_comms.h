#ifndef gemini_comms_h
#define gemini_comms_h

#include <QHostAddress>
#include <QUdpSocket>
#include <QTimer>
#include <QVector>
#include <cstdint>

enum class Gem_cnx_rslt{OK, FAIL_TEMP, FAIL_PERM, TIMEOUT};
enum class Gem_rw_type{READ_INC, READ_FIFO, WR_INC, WR_FIFO};
class Gemini_transaction;
class Gemini_comms;

struct Packet_record;

// Instances of this class are used to read/write Gemini Servers, multiplexing
// access to a single Gemini_comms object that connects to the server because
// Gemini servers can only accept a limited number of connections (ie 3)
// * Create them by calling Gemini_comms::openChannel()
// * Call RwChannel::dispose() to release resources and delete them
class RwChannel: public QObject
{
    Q_OBJECT

    friend class Gemini_comms;
    private:
        Gemini_comms * m_gemComms;
        uint32_t m_ctx;

        RwChannel(Gemini_comms* comms, uint32_t ctx);
        void onRwDone(bool timeout, uint32_t base, uint32_t numregs
                      , QByteArray regs, Gem_rw_type op, bool isAck
                      , uint8_t fail_code);

    public:
        void rw(uint32_t base, uint32_t numRegs, uint32_t *regs
                      , Gem_rw_type opType);
        void dispose();
        ~RwChannel();

    signals:
        void result(bool timedOut, uint32_t base, uint32_t numregs
                    , QByteArray regs, Gem_rw_type op, bool isAck
                    , uint8_t fail_code);
};

// Instances of this class are used to connect with a Gemini Server.
// * After creation, call Gemini_comms::udpConnect and wait for the signal
//   Gemini_comms::cnx_result.
// * If the udpConnect() succeeds, create RwChannels for sending/receiving using
//   Gemini_comms::openChannel()
class Gemini_comms: public QObject
{
    Q_OBJECT

    friend class RwChannel;
    private:
        QHostAddress m_addr;
        uint16_t m_port;
        uint64_t m_timeoutmsec;
        uint32_t m_maxRetries;
        QUdpSocket * m_skt;
        QTimer *m_timer;
        uint32_t m_cnxRetryCount;
        bool m_isConnected;
        uint32_t m_numInTransit;
        bool m_waitingForRetryResponse;
        uint32_t m_pipelineLen;
        uint32_t m_maxPduBytes;
        uint32_t m_maxPayloadWords;

        uint8_t m_cli_seq;
        uint8_t m_svr_seq;

        QVector<RwChannel *> m_chans;

        QList<Gemini_transaction*> m_transList;
        QList<Packet_record*> m_received_trace;

        void startTimeoutTimer(Gemini_transaction * trans);
        void trySendTransactions();
        void resendOldestTransaction();
        void sendGeminiTransaction(Gemini_transaction * trans);
        void sendPdu(char *data, uint32_t len);
        void closeConnection();
        void rw(uint32_t base, uint32_t numRegs, uint32_t *regs
                      , Gem_rw_type opType, uint32_t ctx);
        void recordRxPacket(char buf[], qint64 len);
        void printRxTraceToLog();
    private slots:
        void onCnxTimeout();
        void onCnxRxReady();

        void onPktRxReady();
        void onPktTimeout();
    public:
        Gemini_comms(const Gemini_comms&) = delete; // no copy
        Gemini_comms& operator=(const Gemini_comms &) = delete; // no assign
        Gemini_comms(QHostAddress addr, const uint16_t port
                , const uint64_t timeout_msec
                , const uint32_t retries);
        ~Gemini_comms();


        RwChannel* openChannel();
    public slots:
        void udpConnect(); // Initiate connection to FPGA server
    signals:
        void cnx_result(Gem_cnx_rslt); // Callback result of CNX attempt
        void cnx_failed(); // comms failure
        void read_complete(uint32_t base, uint32_t numregs, QByteArray ba);
        void write_complete(uint32_t base, uint32_t numregs);
};

#endif
