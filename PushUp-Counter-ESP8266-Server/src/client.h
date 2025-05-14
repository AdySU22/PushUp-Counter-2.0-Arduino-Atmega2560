#ifndef CLIENT_H__
#define CLIENT_H__

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <esp_softap.h>
#include <esp_sta.h>
#include <esp_wifi.h>
#include <esp_common.h>

#include <lwip/sockets.h>
#include <lwip/api.h>
#include <lwip/err.h>

#include <freertos/semphr.h> 

// Configs
#define AP_SSID "PUSHUP-BAR-SERVER"
#define AP_PASS "pushup-bar"
#define STA_SSID_DEFAULT "Redmi Note 13 5G"
#define STA_SSID_ALTERNATIVE "LAPTOP-41BJ3APC 3889"
#define STA_SSID STA_SSID_DEFAULT

#define STA_PASS_DEFAULT "7zpn48butmgrmrd"
#define STA_PASS_ALTERNATIVE "12345678"
#define STA_PASS STA_PASS_DEFAULT

#define AP_SSID_LENGTH ((sizeof(AP_SSID)/sizeof(AP_SSID[0])) - 1)
#define AP_PASS_LENGTH ((sizeof(AP_PASS)/sizeof(AP_PASS[0])) - 1)
#define STA_SSID_LENGTH ((sizeof(STA_SSID)/sizeof(STA_SSID[0])) - 1)
#define STA_PASS_LENGTH ((sizeof(STA_PASS)/sizeof(STA_PASS[0])) - 1)
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
#define HEADER_END "\r\n\r\n"
#define MAX_PACKET_SIZE 8192
typedef struct http_packet {
	char buf[MAX_PACKET_SIZE];
	int16_t len;
} http_packet_t;
typedef struct http_request_info {
	char method[8];
	char path[8];
	char version[10];
	int16_t http_return_code;
} http_request_info_t;

extern bool use_open_wifi;
extern int num_clients;
extern conn_config_t *selected_ap;

void print_station_clients(void);
void scan_task(void *arg);
void accept_task(void *arg);
void client_task(void *arg);
int32_t send_data(int client_socket, http_request_info_t *req_info);
void set_use_open_wifi(bool use);

#endif // CLIENT_H__