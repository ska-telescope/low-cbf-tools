/* Code to send LFAA simulation data over a 40Gb network link to Gemini card.
 *
 * Generation of simulated data is described here:
 * https://perentie.atlassian.net/wiki/spaces/MD/pages/58753071/LFAA+simulator
 *
 * The input to this program is two files (headers, data) and UDP destination
 * IP address and port that the data should be sent to. The data file is
 * expected to represent two LFAA streams, overall data rate ~25Gbit/sec
 *
 * Keith Bengston, CSIRO. 21 Jan 2018
 */

#include <iostream> // cout, cin, cerr
#include <unistd.h> // getopt
#include <stdlib.h> // for atoi
#include <cstring>
#include <string>
#include <sys/select.h>
#include <time.h>
#include <errno.h>
#include "lfaa_tx_data.h"
#include <time.h> // for clock_gettime

void usage(char * progname)
{
    std::cout << "USAGE: " << progname << " -h header_file -d data_file"
        << " -a my.ip.dest.addr -p dest_port -r repeats -z fixed_no_of_pkts"
        << std::endl;
}

int main( int argc, char* argv[])
{
    // Gather arguments provided on the command line
    int ret;
    std::string data_file_name;
    std::string hdr_file_name;
    char dest_addr[32] = {'\0'}; // expected in dotted notation eg "10.32.0.1"
    uint16_t port = 0;
    uint32_t repeats=0;
    uint32_t fixed_pkts = 0;
    if(argc < 2)
    {
        std::cout << "No program arguments provided\n" << std::endl;
        usage(argv[0]);
        return 0;
    }
    while((ret = getopt(argc, argv, "z:d:h:a:p:r:?")) != -1)
    {
        switch(ret)
        {
            case 'd':
                data_file_name = std::string(optarg);
                break;
            case 'h':
                hdr_file_name = std::string(optarg);
                break;
            case 'a':
                strncpy(dest_addr, optarg, sizeof(dest_addr));
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'r':
                repeats = atoi(optarg);
                break;
            case 'z':
                fixed_pkts = atoi(optarg);
                break;
            case '?':
                usage(argv[0]);
                return 0;
            default:
                usage(argv[0]);
                return -1;
                break;
        }
    }
    if( data_file_name.size() == 0)
    {
        std::cout << "Error - missing data file name" << std::endl;
        usage(argv[0]);
        return -1;
    }
    if( hdr_file_name.size() == 0)
    {
        std::cout << "Error - missing header file name" << std::endl;
        usage(argv[0]);
        return -1;
    }
    if(strlen(dest_addr) == 0)
    {
        std::cout << "Error - missing destination IP address" << std::endl;
        usage(argv[0]);
        return -1;
    }
    if(port == 0)
    {
        std::cout << "Error - missing destination port number" << std::endl;
        usage(argv[0]);
        return -1;
    }

    // Read data files
    Lfaa_tx_data tx_data;
    tx_data.load_header_file(hdr_file_name);
    tx_data.load_data_file(data_file_name);
    tx_data.set_dest(dest_addr, port);

    uint32_t n_pkts = tx_data.get_num_pkts();
    if((fixed_pkts >0) && (fixed_pkts < n_pkts))
        n_pkts = fixed_pkts;
    struct msghdr * msghdr = tx_data.get_msg_ptr();
    uint64_t * send_dly_us = tx_data.get_send_dly_us();

    // Create sending socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0)
    {
        std::cerr << "ERROR creating socket: " << strerror(errno) << std::endl;
        return -1;
    }

    // Send all the packets
    std::cout<< "Start sending packets" << std::endl;
    timespec ts_start;
    bool have_ts_start = (clock_gettime(CLOCK_MONOTONIC, &ts_start) >= 0);
    uint32_t pkt_sent = 0;
    for(uint32_t rpt=0; rpt<(1+repeats); rpt++)
    {
        for(uint32_t i=0; i<n_pkts; i++)
        {
#if 0
            // How much delay before sending?
            struct timeval waitTime;
            waitTime.tv_usec = send_dly_us[i];
            waitTime.tv_sec = 0;
            while(waitTime.tv_usec > 1000000)
            {
                waitTime.tv_usec -= 1000000;
                waitTime.tv_sec += 1;
            }

            // Delay
            int numEvents;
            do {
                numEvents = select( 0, 0, 0, 0, &waitTime);
            } while((numEvents == -1) && (errno == EINTR));
            if(numEvents < 0)
            {
                std::cerr << "ERROR: select failure: " << strerror(errno)
                    << std::endl;
                return -1;
            }
#endif
            // Send a packet
            sendmsg(sock, &msghdr[i], 0);
        }
        pkt_sent += n_pkts;
    }
    timespec ts_end;
    bool have_ts_end;
    have_ts_end = (clock_gettime(CLOCK_MONOTONIC, &ts_end) >= 0);
    uint32_t usec = 0;
    if(have_ts_start && have_ts_end)
    {
        uint64_t diff_ns;
        uint64_t diff_sec = ts_end.tv_sec - ts_start.tv_sec;
        if(ts_start.tv_nsec > ts_end.tv_nsec)
        {
            diff_sec -= 1;
            diff_ns = (ts_end.tv_nsec + 1000000000) - ts_start.tv_nsec;
        }
        else
        {
            diff_ns = ts_end.tv_nsec - ts_start.tv_nsec;
        }
        usec = diff_sec * 1000000 + (diff_ns/1000);
        std::cout << usec << " usec elapsed" << std::endl;
    }
    std::cout << pkt_sent << " packets sent" << std::endl;

    uint64_t total_bytes = 0;
    for(unsigned int pkt=0; pkt<n_pkts; pkt++)
    {
        // Add up all the payload bytes in the iovecs
        int n_iov = msghdr[pkt].msg_iovlen;
        for(int vec=0; vec<n_iov; vec++)
            total_bytes += msghdr[pkt].msg_iov[vec].iov_len;
        // Add bytes in UDP & IP header
        total_bytes += (20+8);
    }
    total_bytes = total_bytes * (repeats+1);
    std::cout << total_bytes << " bytes sent" << std::endl;
    if(usec != 0)
    {
        float rate = ((float) total_bytes / (float) usec) * 8.0;
        std::cout << "Average sending rate: " << rate << " Mbps" << std::endl;
    }

    std::cout << "done." << std::endl;
}
