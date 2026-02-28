#include "PCAPWriter.h"
#include <M5Cardputer.h>

namespace Vanguard {

PCAPWriter::PCAPWriter(const char* filename) 
    : m_headerWritten(false) 
{
    strncpy(m_filename, filename, sizeof(m_filename) - 1);
}

bool PCAPWriter::open() {
    // If file exists, we don't overwrite global header
    bool exists = SD.exists(m_filename);
    
    m_file = SD.open(m_filename, FILE_APPEND);
    if (!m_file) return false;

    if (!exists) {
        pcap_global_header header;
        header.magic_number = 0xa1b2c3d4;
        header.version_major = 2;
        header.version_minor = 4;
        header.thiszone = 0;
        header.sigfigs = 0;
        header.snaplen = 65535;
        header.network = 105; // IEEE 802.11

        m_file.write((uint8_t*)&header, sizeof(header));
        m_file.flush();
    }
    
    m_headerWritten = true;
    return true;
}

bool PCAPWriter::writePacket(const uint8_t* data, uint16_t len) {
    if (!m_headerWritten && !open()) return false;
    if (!m_file) return false;

    pcap_packet_header pktHeader;
    uint32_t now = millis();
    pktHeader.ts_sec = now / 1000;
    pktHeader.ts_usec = (now % 1000) * 1000;
    pktHeader.incl_len = len;
    pktHeader.orig_len = len;

    // Write header and data to file directly (buffered by File object in most ESP32 SD libs)
    // But we manually flush only when needed to avoid blocking
    m_file.write((uint8_t*)&pktHeader, sizeof(pktHeader));
    m_file.write(data, len);

    if (millis() - m_lastFlushMs > 500) {
        m_file.flush();
        m_lastFlushMs = millis();
    }

    return true;
}

void PCAPWriter::close() {
    if (m_file) {
        m_file.flush();
        m_file.close();
    }
    m_headerWritten = false;
}

void PCAPWriter::flush() {
    if (m_file) m_file.flush();
}

} // namespace Vanguard
