#ifndef VANGUARD_PCAP_WRITER_H
#define VANGUARD_PCAP_WRITER_H

/**
 * @file PCAPWriter.h
 * @brief Simple libpcap file serializer
 */

#include <Arduino.h>
#include "SDManager.h"

namespace Vanguard {

struct pcap_global_header {
    uint32_t magic_number;   /* magic number */
    uint16_t version_major;  /* major version number */
    uint16_t version_minor;  /* minor version number */
    int32_t  thiszone;       /* GMT to local correction */
    uint32_t sigfigs;        /* accuracy of timestamps */
    uint32_t snaplen;        /* max length of captured packets, in octets */
    uint32_t network;        /* data link type */
};

struct pcap_packet_header {
    uint32_t ts_sec;         /* timestamp seconds */
    uint32_t ts_usec;        /* timestamp microseconds */
    uint32_t incl_len;       /* number of octets of packet saved in file */
    uint32_t orig_len;       /* actual length of packet */
};

class PCAPWriter {
public:
    PCAPWriter(const char* filename);
    
    bool open();
    bool writePacket(const uint8_t* data, uint16_t len);
    void close();
    void flush();

private:
    char m_filename[64];
    bool m_headerWritten;
    File m_file;
    uint8_t m_buffer[2048]; // 2KB write buffer
    size_t m_bufferPos = 0;
};

} // namespace Vanguard

#endif // ASSESSOR_PCAP_WRITER_H
