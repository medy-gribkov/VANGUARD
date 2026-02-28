#ifndef MOCK_ESP_WIFI_H
#define MOCK_ESP_WIFI_H

#include <cstdint>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1

typedef enum {
    WIFI_IF_STA = 0,
    WIFI_IF_AP
} wifi_interface_t;

typedef enum {
    WIFI_SECOND_CHAN_NONE = 0
} wifi_second_chan_t;

typedef enum {
    WIFI_PKT_MGMT = 0,
    WIFI_PKT_CTRL,
    WIFI_PKT_DATA,
    WIFI_PKT_MISC
} wifi_promiscuous_pkt_type_t;

typedef void (*wifi_promiscuous_cb_t)(void* buf, wifi_promiscuous_pkt_type_t type);

inline esp_err_t esp_wifi_set_channel(uint8_t, wifi_second_chan_t) { return ESP_OK; }
inline esp_err_t esp_wifi_set_promiscuous(bool) { return ESP_OK; }
inline esp_err_t esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_cb_t) { return ESP_OK; }
inline esp_err_t esp_wifi_80211_tx(wifi_interface_t, const void*, int, bool) { return ESP_OK; }

#endif // MOCK_ESP_WIFI_H
