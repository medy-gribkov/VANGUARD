#ifndef VANGUARD_BRUCE_IR_H
#define VANGUARD_BRUCE_IR_H

/**
 * @file BruceIR.h
 * @brief IR adapter for Cardputer (Core: IRremote)
 */

#include <Arduino.h>
#include "../core/VanguardTypes.h"

namespace Vanguard {

class BruceIR {
public:
    static BruceIR& getInstance();

    bool init();
    void shutdown();
    void tick();

    // Transmission
    void sendRaw(const uint16_t* data, uint16_t len, uint16_t khz = 38);
    void sendTVBGone(); // Power cycle for common TVs

    // Recording
    void startRecording();
    void stopRecording();
    bool hasLastCapture() const { return m_hasLastCapture; }
    void replayLast();

private:
    BruceIR();
    
    bool m_initialized;
    bool m_recording;
    bool m_hasLastCapture;

    // Stored capture data for replay (avoid IRData in header)
    uint8_t m_lastProtocol;      // decode_type_t cast to uint8_t
    uint16_t m_lastAddress;
    uint16_t m_lastCommand;
    uint8_t m_lastFlags;
    uint16_t m_rawBuffer[256];   // Raw timing data backup
    uint16_t m_rawLen;
};

} // namespace Vanguard

#endif // VANGUARD_BRUCE_IR_H
