#include "BruceIR.h"
#include <IRremote.hpp>
#include <M5Cardputer.h>

namespace Vanguard {

BruceIR::BruceIR()
    : m_initialized(false)
    , m_recording(false)
    , m_hasLastCapture(false)
    , m_lastProtocol(0)
    , m_lastAddress(0)
    , m_lastCommand(0)
    , m_lastFlags(0)
    , m_rawLen(0)
{
    memset(m_rawBuffer, 0, sizeof(m_rawBuffer));
}

BruceIR& BruceIR::getInstance() {
    static BruceIR instance;
    return instance;
}

bool BruceIR::init() {
    if (m_initialized) return true;

    // Cardputer IR Pins
    // TX: 44, RX: 43
    
    // Initialize IRremote
    // TX pin is defined via IrSender.begin(TX_PIN)
    // RX pin is defined via IrReceiver.begin(RX_PIN)
    
    IrSender.begin(44);
    IrReceiver.begin(43, DISABLE_LED_FEEDBACK); // RX on pin 43, no LED (RMT conflict)

    if (Serial) {
        Serial.println("[IR] Initialized (TX:44, RX:43)");
    }

    m_initialized = true;
    return true;
}

void BruceIR::tick() {
    if (!m_initialized || !m_recording) return;

    if (IrReceiver.decode()) {
        if (Serial) {
            Serial.println("[IR] Signal captured!");
            IrReceiver.printIRResultShort(&Serial);
        }
        
        m_hasLastCapture = true;
        // Store decoded data for replay
        m_lastProtocol = (uint8_t)IrReceiver.decodedIRData.protocol;
        m_lastAddress = IrReceiver.decodedIRData.address;
        m_lastCommand = IrReceiver.decodedIRData.command;
        m_lastFlags = IrReceiver.decodedIRData.flags;
        
        stopRecording(); // Stop after first capture for now
        IrReceiver.resume();
    }
}

void BruceIR::sendRaw(const uint16_t* data, uint16_t len, uint16_t khz) {
    if (!m_initialized) return;
    
    if (Serial) Serial.printf("[IR] Sending raw (%u pulses)\n", len);
    IrSender.sendRaw(data, len, khz);
}

void BruceIR::sendTVBGone() {
    if (!m_initialized) return;
    
    if (Serial) Serial.println("[IR] Running TV-B-Gone sequence...");
    
    // Common Power Codes
    
    // 1. NEC: Samsung Power
    // Address: 0x0707, Command: 0x02
    IrSender.sendNEC(0x0707, 0x02, 1);
    delay(50);
    
    // 2. NEC: LG Power
    // Address: 0x04, Command: 0x08
    IrSender.sendNEC(0x04, 0x08, 1);
    delay(50);

    // 3. Sony: Power (12-bit)
    // Address: 0x01, Command: 0x15
    IrSender.sendSony(0x01, 0x15, 2, 12); // 2 repeats
    delay(50);
    
    // 4. NEC: Generic Power
    // Address: 0x00, Command: 0x45
    IrSender.sendNEC(0x00, 0x45, 1);
    delay(50);

    // 5. RC5: Philips Power
    // Address: 0x00, Command: 0x0C
    IrSender.sendRC5(0x00, 0x0C, 0, 1);
    delay(50);

    if (Serial) Serial.println("[IR] TV-B-Gone sequence complete.");
}

void BruceIR::startRecording() {
    if (!m_initialized) return;
    m_recording = true;
    m_hasLastCapture = false;
    IrReceiver.resume();
    if (Serial) Serial.println("[IR] Recording started...");
}

void BruceIR::stopRecording() {
    m_recording = false;
    if (Serial) Serial.println("[IR] Recording stopped.");
}

void BruceIR::shutdown() {
    if (!m_initialized) return;
    m_recording = false;
    IrReceiver.stop();
    m_initialized = false;
    if (Serial) Serial.println("[IR] Shutdown complete");
}

void BruceIR::replayLast() {
    if (!m_initialized || !m_hasLastCapture) return;

    if (Serial) Serial.println("[IR] Replaying last capture...");

    // Reconstruct IRData from stored fields
    IRData data;
    memset(&data, 0, sizeof(data));
    data.protocol = (decode_type_t)m_lastProtocol;
    data.address = m_lastAddress;
    data.command = m_lastCommand;
    data.flags = m_lastFlags;
    data.numberOfBits = 0; // write() infers from protocol

    IrSender.write(&data);
}

} // namespace Vanguard
