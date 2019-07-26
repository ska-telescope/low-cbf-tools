/* This class allocates and holds memory for all the LFAA simulation packets.
 * It also creates msghdrs so that the data is easily sent via sendmsg().
 *
 * Keith Bengston. CSIRO. 21 January 2018.
 */


#ifndef LFAA_TX_DATA_H
#define LFAA_TX_DATA_H

#include <string>
#include <sys/types.h>  // for sendmsg
#include <sys/socket.h> // for iovec and msghdr
#include <arpa/inet.h>  // for sockaddr_in
#include <memory>       // for unique_ptr

class Lfaa_tx_data
{
    private:
        bool m_is_hdr_ok;
        bool m_is_data_ok;
        struct sockaddr_in m_dest;
        // Array of message headers - one entry per message
        std::unique_ptr<struct msghdr[]> m_msghdr;
        // Array of iovec structures - two entries used per message
        std::unique_ptr<struct iovec[]> m_iovec;
        // SPEAD part of payload for each packet
        uint32_t m_num_pkts;
        std::unique_ptr<char[]>m_hdr;
        // data part of payload for each packet
        uint64_t m_payload_len;
        char m_zero[8192] = {0};
        std::unique_ptr<char[]>m_payload;
        std::unique_ptr<uint64_t[]> m_send_dly_us;

        static uint64_t big_endian_64bit(uint8_t * ptr);
        static uint32_t big_endian_32bit(uint8_t * ptr);

    public:
        Lfaa_tx_data();
        ~Lfaa_tx_data();
        bool load_header_file(std::string file);
        bool load_data_file(std::string file);
        uint32_t get_num_pkts();
        struct msghdr * get_msg_ptr();
        uint64_t * get_send_dly_us();
        bool set_dest(char * destination, uint16_t port);
};

#endif
