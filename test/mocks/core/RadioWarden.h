#ifndef MOCK_RADIO_WARDEN_H
#define MOCK_RADIO_WARDEN_H

namespace Vanguard {

enum class RadioOwner { OWNER_NONE, OWNER_WIFI_STA, OWNER_WIFI_AP, OWNER_BLE };

class RadioWarden {
public:
    static RadioWarden& getInstance() { static RadioWarden i; return i; }
    void requestRadio(RadioOwner owner) {}
    void releaseRadio() {}
};

}

#endif
