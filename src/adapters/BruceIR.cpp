#include "BruceIR.h"
#include <IRremote.hpp>
#include <M5Cardputer.h>

namespace Vanguard {

BruceIR::BruceIR() : m_initialized(false), m_recording(false), m_hasLastCapture(false) {}

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
    IrReceiver.begin(43, ENABLE_LED_FEEDBACK); // Enable RX on pin 43

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
        // Logic to store last capture for replay would go here
        // IrReceiver.decodedIRData holds the result
        
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

void BruceIR::replayLast() {
    if (!m_initialized || !m_hasLastCapture) return;
    
    if (Serial) Serial.println("[IR] Replaying last capture...");
    // Replay logic using IrSender.write(&IrReceiver.decodedIRData)
    IrSender.write(&IrReceiver.decodedIRData);
}

} // namespace Vanguard
