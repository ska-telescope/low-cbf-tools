#include "lfaa_tx_data.h"
#include "bigfile.h"
#include <cassert>
#include <iostream> // for cin cout cerr
#include <memory> // for make_unique
#include <cstring> // for memcpy

#define SPEAD_HDR_LEN 72

// Structure in the model-generated header file
struct Lfaa_hdr_t
{
    uint8_t data_offset[8];
    uint8_t hdr_data_len_bytes[4]; // FIXME length of hdr or payload??
    uint8_t send_time_ns[8];// TODO move to front of struct for better alignment
    uint8_t reserved[12];
    uint8_t eth_hdr[14];
    uint8_t ip_hdr[20];
    uint8_t udp_hdr[8];
    uint8_t spead_hdr[SPEAD_HDR_LEN];
    uint8_t unused_pad[2];
} __attribute__((packed)) ; // note: GCC-specific keyword (avoids padding)




Lfaa_tx_data::Lfaa_tx_data()
    : m_is_hdr_ok(false)
    , m_is_data_ok(false)
{
}


Lfaa_tx_data::~Lfaa_tx_data()
{
}

uint64_t Lfaa_tx_data::big_endian_64bit(uint8_t * ptr)
{
    uint64_t val = 0;
    for(int i=0; i<8; i++)
        val = (val << 8) | ptr[i];
    return val;
}

uint32_t Lfaa_tx_data::big_endian_32bit(uint8_t * ptr)
{
    uint32_t val = 0;
    for(int i=0; i<4; i++)
        val = (val << 8) | ptr[i];
    return val;
}

bool Lfaa_tx_data::load_header_file(std::string file)
{
    m_is_hdr_ok = false;
    assert(sizeof(Lfaa_hdr_t) == 148); // our type matches Matlab size?

    // Read header file into temporary variable
    Bigfile data(file);
    bool data_ok = data.read();
    if(!data_ok)
    {
        std::cout << "Error - Unable to read data file '"
            << file << "'" << std::endl;
        return false;
    }
    uint64_t hdr_data_len = data.size();
    assert( (hdr_data_len % sizeof(Lfaa_hdr_t)) == 0); // no partial headers?
    m_num_pkts = hdr_data_len / sizeof(Lfaa_hdr_t);
    std::cout << "Header file contains " << m_num_pkts << " headers" << std::endl;
    
    // Move all the data into this class
    m_hdr = data.data();
    Lfaa_hdr_t * hdr_data_ptr = reinterpret_cast<Lfaa_hdr_t *>(m_hdr.get());

    // Allocate extra space we'll need to hold the structures used to send data
    // as UDP packets via sendmsg() call
    try
    {
        m_msghdr = std::make_unique<struct msghdr[]>(m_num_pkts);
        m_iovec = std::make_unique<struct iovec[]>(m_num_pkts * 2);
        m_send_dly_us = std::make_unique<uint64_t[]>(m_num_pkts);
    }
    catch (std::bad_alloc &ba)
    {
        std::cerr << "Couldn't allocate RAM for header data: " << ba.what()
            << std::endl;
        return false;
    }
    
    // Fill in all the message headers as far as the SPEAD headers
    for(unsigned int idx = 0; idx < m_num_pkts; idx++)
    {
        // Fill in message header with destination and iovec pointer 
        memset(&m_msghdr[idx], 0, sizeof(struct msghdr));
        m_msghdr[idx].msg_name = &m_dest;
        m_msghdr[idx].msg_namelen = sizeof(m_dest);
        m_msghdr[idx].msg_iov = &m_iovec[2*idx]; 
        m_msghdr[idx].msg_iovlen = 2; // two iov entries: header + data
        
        // make first iovec structure point to SPEAD header data
        m_iovec[2*idx].iov_base = hdr_data_ptr[idx].spead_hdr;
        //hdr_data_ptr[idx].spead_hdr[0] = idx % 0x7f; // kb DEBUG TEST COUNTER
        m_iovec[2*idx].iov_len = SPEAD_HDR_LEN;
    }

    m_is_hdr_ok = true;
    return true;
}

struct channel_list
{
    uint32_t station;
    std::list<uint32_t> freq_chans;
};

void Lfaa_tx_data::add_freq_channel( std::list<channel_list> *cl
        , uint32_t station, uint32_t chan)
{
    for( auto it = cl->begin(); it!=cl->end(); ++it)
    {
        if(it->station == station)
        {
            for( auto chan_in_use: it->freq_chans)
            {
                if(chan_in_use == chan)
                    return;
            }
            it->freq_chans.push_back(chan);
            return;
        }
    }
    channel_list c;
    c.station = station;
    c.freq_chans.push_back(chan);
    cl->push_back(c);
    return;
}


bool Lfaa_tx_data::load_data_file(std::string file)
{
    m_is_data_ok = false;

    // Read data file into memory
    Bigfile data(file);
    bool data_ok = data.read();
    if(!data_ok)
    {
        std::cout << "Error - Unable to read data file '"
            << file << "'" << std::endl;
        return false;
    }
    m_payload_len = data.size();
    m_payload = data.data();

    Lfaa_hdr_t * hdr_data_ptr = reinterpret_cast<Lfaa_hdr_t *>(m_hdr.get());
    uint64_t last_send_ns = 0;
    std::list<channel_list> in_use;
    for(unsigned int idx=0; idx<m_num_pkts; idx++)
    {
        // for each station, create a list of channels it is sending
        uint32_t stationID = hdr_data_ptr[idx].spead_hdr[104-43];
        stationID += (hdr_data_ptr[idx].spead_hdr[103-43] << 8);
        uint32_t logicalChan = hdr_data_ptr[idx].spead_hdr[11];
        logicalChan += (hdr_data_ptr[idx].spead_hdr[10] << 8);
        add_freq_channel(&in_use, stationID, logicalChan);
#if 0
        // debug
        {
            uint32_t substationID = hdr_data_ptr[idx].spead_hdr[101-43];
            uint32_t subarrayIdx = hdr_data_ptr[idx].spead_hdr[102-43];

            std::cout << big_endian_64bit(hdr_data_ptr[idx].send_time_ns)
            << "," << stationID
            << "," << substationID
            << "," << subarrayIdx
            << "," << logicalChan
            << std::endl;
        }
#endif

        //Fill in second iovec entry
        uint64_t offset = big_endian_64bit(hdr_data_ptr[idx].data_offset);
        uint32_t len = big_endian_32bit(hdr_data_ptr[idx].hdr_data_len_bytes);
        if(offset == 0xffffffffffffffff)
        {
            m_iovec[2*idx+1].iov_base = &m_zero;
        }
        else
        {
            if ((offset+len) > m_payload_len)
            {
                // matlab error
                std::cerr << "Error in header info" << std::endl;
                std::cerr << "hdr[" << idx
                    << "] offset=" << offset << " len=" << len
                    << " size=" << m_payload_len << std::endl;
                return false;
            }
            m_iovec[2*idx+1].iov_base = &m_payload[offset];
        }
        m_iovec[2*idx+1].iov_len = len;

        // Fill in delta send time
        if(idx == 0)
        {
            m_send_dly_us[idx] = 0;
            last_send_ns = big_endian_64bit(hdr_data_ptr[idx].send_time_ns);
        }
        else
        {
            uint64_t send_dly_ns =
                big_endian_64bit(hdr_data_ptr[idx].send_time_ns) - last_send_ns;
            uint64_t send_dly_us = send_dly_ns / 1000;
            m_send_dly_us[idx] = send_dly_us;
            last_send_ns += (send_dly_us * 1000);
        }
    }
    std::cout << "Message headers and iovecs created" << std::endl;

    m_is_data_ok = true;
    m_num_freq_chans = 0;
    for(auto ch: in_use)
        m_num_freq_chans += ch.freq_chans.size();
    std::cout << "Total of " << m_num_freq_chans
        << " coarse channels for " << in_use.size() << " stations" << std::endl;
    return true;
}

uint32_t Lfaa_tx_data::get_num_pkts()
{
    return m_num_pkts;
}


struct msghdr * Lfaa_tx_data::get_msg_ptr()
{
    return m_msghdr.get();
}

uint64_t * Lfaa_tx_data::get_send_dly_us()
{
    return m_send_dly_us.get();
}

bool Lfaa_tx_data::set_dest(char * destination, uint16_t port)
{
    memset(&m_dest, 0, sizeof(m_dest));
    m_dest.sin_family = AF_INET;

    int rv = inet_pton(AF_INET, destination, &m_dest.sin_addr.s_addr);
    if(rv <= 0)
    {
        std::cout << "Bad IP address: " << destination << std::endl;
        return false;
    }

    //m_dest.sin_addr.s_addr = htonl(INADDR_ANY);
    m_dest.sin_port = htons(port);

    return true;
}

uint32_t Lfaa_tx_data::get_num_freq_chans()
{
    return m_num_freq_chans;
}
