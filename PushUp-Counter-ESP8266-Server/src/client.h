#ifndef CLIENT_H__
#define CLIENT_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <esp_softap.h>
#include <esp_sta.h>
#include <esp_wifi.h>
#include <lwip/sockets.h>
#include <lwip/api.h>
#include <esp_common.h>
#include <stdbool.h>
#include <errno.h>
#include <lwip/err.h>
#include <freertos/semphr.h> 

// Configs
#define AP_SSID "PUSHUP-BAR-SERVER"
#define AP_PASS "pushup-bar"
#define STA_SSID_DEFAULT "Redmi Note 13 5G"
#define STA_SSID STA_SSID_DEFAULT
#define STA_PASS_DEFAULT "7zpn48butmgrmrd"
#define STA_PASS STA_PASS_DEFAULT
#define STA_BSSID_DEFAULT {0x14, 0x99, 0x3E, 0xA5, 0xB1, 0x4B}
#define STA_BSSID STA_BSSID_DEFAULT
#define AP_SSID_LENGTH ((sizeof(AP_SSID)/sizeof(AP_SSID[0])) - 1)
#define AP_PASS_LENGTH ((sizeof(AP_PASS)/sizeof(AP_PASS[0])) - 1)
// Wi-Fi Channel number 
#define CHANNEL 1
// Maximum number of client sockets
#define MAX_CLIENT_NUM 4

// Typedefs
typedef struct softap_config ap_config_t;
typedef struct scan_config scan_config_t;
typedef struct station_config sta_config_t;
typedef struct station_info sta_info_t;
typedef struct bss_info bss_info_t;
typedef enum dhcp_status dchp_status_t;
typedef struct connection_config {
	sta_config_t sta_conf;
	int8_t rssi;
} conn_config_t; 

extern bool use_open_wifi;
extern int num_clients;
extern conn_config_t *selected_ap;

void print_station_clients(void);
void scan_task(void *arg);
void accept_task(void *arg);
void client_task(void *arg);
int32_t send_data(uint16_t data);
void set_use_open_wifi(bool use);

#endif // CLIENT_H__