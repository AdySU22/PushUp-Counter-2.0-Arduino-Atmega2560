#include "client.h"
#include "spi.h"

#define MAX(x, y) ((x) < (y) ? (y) : (x))

extern xSemaphoreHandle scan_semaphore; 
extern xSemaphoreHandle accept_semaphore;
extern xSemaphoreHandle client_list_mutex;
bool use_open_wifi = false;
conn_config_t *selected_ap = NULL;

static int server_sock, active_client_socks[MAX_CLIENT_NUM], processed_client_socks[MAX_CLIENT_NUM], processed_client_socks_len;
int *active_ptr = &active_client_socks[0], *processed_ptr = &processed_client_socks[0];
int num_clients;

static http_packet_t packet;
static http_request_info_t req_info;

void set_use_open_wifi(bool use) {
	use_open_wifi = use;
}

void scan_callback(void *arg, STATUS status) {
	os_printf("[%s] Scan status: ", __FUNCTION__);
	switch (status) {
		case OK: {
			os_printf("OK\n");
			bss_info_t *bss_info = (bss_info_t*)arg;
			while (bss_info) {
				os_printf("[%s] BSSID / MAC: %d:%d:%d:%d:%d:%d\nSSID: %s\nChannel: %d\nRSSI: %d\nAuth Mode: ", 
					__FUNCTION__, bss_info->bssid[0], bss_info->bssid[1], bss_info->bssid[2], bss_info->bssid[3], bss_info->bssid[4], bss_info->bssid[5],
					bss_info->ssid, bss_info->channel, bss_info->rssi
				);
				switch (bss_info->authmode) {
					case AUTH_OPEN: {
						os_printf("OPEN\n");
						if (use_open_wifi && bss_info->rssi != 31 && bss_info->rssi >= selected_ap->rssi) {
							selected_ap->sta_conf.bssid_set = 0;
							selected_ap->rssi = bss_info->rssi;
							memcpy(selected_ap->sta_conf.ssid, bss_info->ssid, strlen(bss_info->ssid));
						}
					} break;
					case AUTH_WEP: {
						os_printf("WEP\n");
					} break;
					case AUTH_WPA_PSK: {
						os_printf("WPA\n");
					} break;
					case AUTH_WPA2_PSK: {
						os_printf("WPA2\n");
					} break;
					case AUTH_WPA_WPA2_PSK: {
						os_printf("WPA / WPA2\n");
					} break;
					default: 
						break;
				}
				os_printf("[%s] Hidden: %s\nFrequency Offset: %d\nFrequency Calibration: %d\n\n", __FUNCTION__, bss_info->is_hidden ? "True" : "False", bss_info->freq_offset, bss_info->freqcal_val);
				bss_info = bss_info->next.stqe_next;
			}
		} break;
		case PENDING: {
			os_printf("[%s] Pending\n", __FUNCTION__);
		}	break;
		case FAIL: {
			os_printf("[%s] Failed\n", __FUNCTION__);
		} break;
		case BUSY: {
			os_printf("[%s] Busy\n", __FUNCTION__);
		} break;
		case CANCEL: {
			os_printf("[%s] Cancelled\n", __FUNCTION__);
		} break;
		default:
			break;
	}
	vTaskDelay(100 / portTICK_RATE_MS);
}

int32_t init_server(void) {
	if (!wifi_set_opmode_current(STATIONAP_MODE)) return 1;
	selected_ap = os_malloc(sizeof(conn_config_t));
	ap_config_t ap_conf = {
		.ssid_len = AP_SSID_LENGTH,
		.channel = CHANNEL,
		.authmode = AUTH_WPA_WPA2_PSK,
		.beacon_interval = 100, // Broadcast every 100 ms
		.max_connection = 4
	};
	memcpy(ap_conf.ssid, AP_SSID, AP_SSID_LENGTH);
	memcpy(ap_conf.password, AP_PASS, AP_PASS_LENGTH);
	if (!wifi_softap_set_config_current(&ap_conf)) return 2;
	scan_config_t scan_conf = {
		.bssid = NULL,
		.channel = 0,
		.show_hidden = true,
	};
	wifi_station_scan(&scan_conf, scan_callback);
	dchp_status_t dhcp = wifi_softap_dhcps_status();
	os_printf("[%s] dhcp status: %s\n", __FUNCTION__, dhcp == DHCP_STARTED ? "Started" : "Stopped");
	os_printf("[%s] WiFi AP SSID: %s\nWifi Password: %s\n", __FUNCTION__, AP_SSID, AP_PASS);
	sta_config_t sta_conf = {};
	memcpy(sta_conf.password, STA_PASS, strlen(STA_PASS));
	memcpy(sta_conf.ssid, STA_SSID, strlen(STA_SSID));
	*selected_ap = (conn_config_t){
		.sta_conf = sta_conf,
		.rssi = 31
	};
	if (!wifi_station_set_config(&sta_conf)) {
		return 4;
	}
	os_printf("[%s] Connecting to %s\n", __FUNCTION__, STA_SSID);
	if (!wifi_station_connect()) { 
		os_printf("[%s] Failed to initiate station connection!\n", __FUNCTION__); 
		return 5;
	}
	switch (wifi_station_get_connect_status()) {
		case STATION_IDLE: {
			os_printf("[%s] Idle\n", __FUNCTION__);
		} break;
		case STATION_CONNECTING: {
			os_printf("[%s] Connecting\n", __FUNCTION__);
		} break;
		case STATION_WRONG_PASSWORD: {
			os_printf("[%s] Wrong password\n", __FUNCTION__);
		} break;
		case STATION_NO_AP_FOUND: {
			os_printf("[%s] No AP found\n", __FUNCTION__);
		} break;
		case STATION_CONNECT_FAIL: {
			os_printf("[%s] Failed to connect\n", __FUNCTION__);
		} break;
		case STATION_GOT_IP: {
			os_printf("[%s] Got IP\n", __FUNCTION__);
		} break;
	}
	return init_sockets();
}

void print_station_clients(void) {
	os_printf("[%s] Current clients:\n", __FUNCTION__);
	sta_info_t *sta_info =  wifi_softap_get_station_info();
	if (!sta_info) {
		os_printf("[%s] No clients.\n", __FUNCTION__); 
		return;
	}
	sta_info_t *curr = sta_info;
	while (curr) {
		os_printf("[%s] BSSID / MAC: %d:%d:%d:%d:%d:%d, \nIP: %d.%d.%d.%d\n", __FUNCTION__, curr->bssid[0], 
			curr->bssid[1], curr->bssid[2], curr->bssid[3], curr->bssid[4], curr->bssid[5],
			((curr->ip.addr) & 0xFF), ((curr->ip.addr >> 8) & 0xFF), ((curr->ip.addr >> 16) & 0xFF), ((curr->ip.addr >> 24) & 0xFF));
		curr = curr->next.stqe_next;
		vTaskDelay(100 / portTICK_RATE_MS);
	}
	wifi_softap_free_station_info();
}

int32_t init_sockets(void) {
	int res = 0;
	int option = 1;
	os_printf("[%s] errno: %p\n", __FUNCTION__, errno);
	server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_sock < 0) {
		printf("Error creating server socket: %s\n", __FUNCTION__, strerror(errno));
		return errno;	
	}
	
	res = setsockopt(server_sock, SOL_SOCKET, SO_KEEPALIVE, &option, sizeof(option));
	if (res < 0) {
		os_printf("[%s] Error setting SO_KEEPALIVE option for server socket: %s\n", __FUNCTION__, strerror(errno));
		return errno;
	}
	
	int keep_idle = 30; // Time in seconds until the first probe on an idle connection
	res = setsockopt(server_sock, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
	if (res < 0) {
		os_printf("[%s] Error setting TCP_KEEPIDLE option for server socket: %s\n", __FUNCTION__, strerror(errno));
		return errno;
	} 
	
	// Set interval between probes
	int keep_intvl = 5; // Time in seconds between subsequent probes
	res = setsockopt(server_sock, IPPROTO_TCP, TCP_KEEPINTVL, &keep_intvl, sizeof(keep_intvl));
	if (res < 0) {
		os_printf("[%s] Error setting TCP_KEEPINTVL option for server socket: %s\n", __FUNCTION__, strerror(errno));
		return errno;
	}
	
	// Set number of probes before dropping connection
	int keep_cnt = 3; // Number of probes before connection is considered dead
	res = setsockopt(server_sock, IPPROTO_TCP, TCP_KEEPCNT, &keep_cnt, sizeof(keep_cnt));
	if (res < 0) {
		os_printf("[%s] Error setting TCP_KEEPCNT option for server socket: %s\n", __FUNCTION__, strerror(errno));
		return errno;
	}
	
	int flags = fcntl(server_sock, F_GETFL, 0);
	if (flags < 0) {
		os_printf("[%s] Error getting socket flags: %s\n", __FUNCTION__, strerror(errno));
		return errno;
	}
	
	// Set non-blocking flag
	flags |= O_NONBLOCK; 
	if (fcntl(server_sock, F_SETFL, flags) < 0) {
		os_printf("[%s] Error setting socket flags: %s\n", __FUNCTION__, strerror(errno));
		return errno;
	}
	
	struct sockaddr_in address = {
		.sin_len = sizeof(struct sockaddr_in),
		.sin_family  = AF_INET,
		.sin_addr = {
			.s_addr = INADDR_ANY
		},
		.sin_port = htons(80)
	};

	res = bind(server_sock, (struct sockaddr*)&address, sizeof(struct sockaddr));
	if (res < 0) {
		os_printf("[%s] Error listening to %d: %s\n", __FUNCTION__, 0, strerror(errno));
		return errno;
	}

	struct sockaddr_in bound_address;
    socklen_t bound_address_len = sizeof(bound_address);
	res = lwip_getsockname(server_sock, (struct sockaddr *)&bound_address, &bound_address_len);
	if (res < 0) {
		os_printf("[%s] Failed to get server's socket address: %s\n", __FUNCTION__, strerror(errno));
		return errno;
	}

	res = listen(server_sock, 4);
	int8_t ip_address[4] = {
		((bound_address.sin_addr.s_addr >> 24) & 0xFF), 
		((bound_address.sin_addr.s_addr >> 16) & 0xFF), 
		((bound_address.sin_addr.s_addr >> 8) & 0xFF), 
		bound_address.sin_addr.s_addr & 0xFF
	};
	if (res < 0) {
		os_printf("[%s] Error listening to %d.%d.%d.%d:%d: %s\n", __FUNCTION__, ip_address[3], ip_address[2], ip_address[1], ip_address[0], ntohs(bound_address.sin_port), strerror(errno));
		return errno;
	}
	os_printf("[%s] Server listening at %d.%d.%d.%d:%d\n", __FUNCTION__, ip_address[3], ip_address[2], ip_address[1], ip_address[0], ntohs(bound_address.sin_port));
	return 0;
}

void scan_task(void *arg) {
	while (1) {
		if (xSemaphoreTake(scan_semaphore, portMAX_DELAY) != pdTRUE) {
			os_printf("[%s] %d FATAL: Failed to take scan_semaphore.\n", __FUNCTION__, __LINE__);
			while(1) vTaskDelay(1000);
		}
		os_printf("[%s] Max bytes used: %d\n", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL));
		os_printf("[%s] Heap bytes remaining: %u\n", __FUNCTION__, xPortGetFreeHeapSize());
		os_printf("[%s] Connected to %s\n", __FUNCTION__, selected_ap->rssi != 31 ? (char*)selected_ap->sta_conf.ssid : "None");
		os_printf("[%s] Connection Status: ", __FUNCTION__);
		switch (wifi_station_get_connect_status()) {
			case STATION_IDLE: {
				os_printf("Idle\n");
			} break;
			case STATION_CONNECTING: {
				os_printf("Connecting\n");
			} break;
			case STATION_WRONG_PASSWORD: {
				os_printf("Wrong password\n");
			} break;
			case STATION_NO_AP_FOUND: {
				os_printf("No AP found\n");
			} break;
			case STATION_CONNECT_FAIL: {
				os_printf("Failed to connect\n");

			} break;
			case STATION_GOT_IP: {
				os_printf("Got IP\n");
			} break;
		}
		scan_config_t scan_conf = {
			.bssid = NULL,
			.channel = CHANNEL,
			.show_hidden = true,
		};
		wifi_station_scan(&scan_conf, scan_callback);
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
}

void accept_task(void *arg) {
	while (1) {
		if (xSemaphoreTake(accept_semaphore, portMAX_DELAY) != pdTRUE) {
			os_printf("[%s] %d FATAL: Failed to take accept_semaphore.\n", __FUNCTION__, __LINE__);
			while (1) vTaskDelay(1000);
		}
		os_printf("[%s] Max bytes used: %d\n", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL));
		os_printf("[%s] Heap bytes remaining: %u\n", __FUNCTION__, xPortGetFreeHeapSize());
		
		struct sockaddr address = {};
		socklen_t address_len = sizeof(struct sockaddr);
		char addr_str[128] = {};
		char *ptr = NULL;
		int client = accept(server_sock, &address, &address_len);
		switch (address.sa_family) {
			case AF_INET: {
				struct sockaddr_in *address_in = (struct sockaddr_in*)&address;
				ptr = inet_ntoa_r(address_in->sin_addr, addr_str, 128);
			} break;
			case AF_INET6: {
				struct sockaddr_in6 *address_in = (struct sockaddr_in6*)&address;
				ptr = inet6_ntoa_r(address_in->sin6_addr, addr_str, 128);
			} break;
			default:
				break;
		}
		if (client < 0) {
			if (errno != EWOULDBLOCK) {
				os_printf("[%s] Failed to accept client due to %d: %s\n", __FUNCTION__, errno, strerror(errno));
			} else {
				os_printf("[%s] Call to accept() would block.\n", __FUNCTION__);
			}
			if (xSemaphoreGive(accept_semaphore) != pdTRUE) {
				os_printf("[%s] %d FATAL: Failed to give accept_semaphore.\n", __FUNCTION__, __LINE__);
				while (1) vTaskDelay(1000);
			}
			goto accept_end;
		} else if (num_clients < MAX_CLIENT_NUM) {
			os_printf("[%s] received address <%s>\n", __FUNCTION__, ptr ? ptr : "None");
			if (xSemaphoreTake(client_list_mutex, portMAX_DELAY) != pdTRUE) {
				os_printf("[%s] %d FATAL: Failed to take client_list_mutex.\n", __FUNCTION__, __LINE__);
				while (1) vTaskDelay(1000);
			}
			os_printf("[%s] Accepted client %d.\n", __FUNCTION__, client, __FUNCTION__);
			int option = 1;
			int res = setsockopt(client, SOL_SOCKET, SO_KEEPALIVE, &option, sizeof(option));
			if (res < 0) {
				os_printf("[%s] Error setting SO_KEEPALIVE option for client socket %d: %s\n", __FUNCTION__, num_clients, strerror(errno));
				goto accept_cleanup;
			} 
			int keep_idle = 30; // Time in seconds until the first probe on an idle connection
			res = setsockopt(client, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
			if (res < 0) {
				os_printf("[%s] Error setting TCP_KEEPIDLE option for server socket: %s\n", __FUNCTION__, strerror(errno));
				goto accept_cleanup;
			} 
			
			// Set interval between probes
			int keep_intvl = 5; // Time in seconds between subsequent probes
			res = setsockopt(client, IPPROTO_TCP, TCP_KEEPINTVL, &keep_intvl, sizeof(keep_intvl));
			if (res < 0) {
				os_printf("[%s] Error setting TCP_KEEPINTVL option for server socket: %s\n", __FUNCTION__, strerror(errno));
				goto accept_cleanup;
			}
			
			// Set number of probes before dropping connection
			int keep_cnt = 3; // Number of probes before connection is considered dead
			res = setsockopt(client, IPPROTO_TCP, TCP_KEEPCNT, &keep_cnt, sizeof(keep_cnt));
			if (res < 0) {
				os_printf("[%s] Error setting TCP_KEEPCNT option for server socket: %s\n", __FUNCTION__, strerror(errno));
				goto accept_cleanup;
			}
			
			int flags = fcntl(client, F_GETFL, 0);
			if (flags < 0) {
				os_printf("[%s] Error getting socket flags: %s\n", __FUNCTION__, strerror(errno));
				goto accept_cleanup;
			}
			
			// Set non-blocking flag
			flags |= O_NONBLOCK;
			
			if (fcntl(client, F_SETFL, flags) < 0) {
				os_printf("[%s] Error setting socket flags: %s\n", __FUNCTION__, strerror(errno));
				goto accept_cleanup;
			}
			active_ptr[num_clients++] = client;
			os_printf("[%s] Number of clients: %d\n", __FUNCTION__, num_clients);
			if (xSemaphoreGive(client_list_mutex) != pdTRUE) {
				os_printf("[%s] %d FATAL: Failed to give client_list_mutex.\n", __FUNCTION__, __LINE__);
			}
			goto accept_end;
		}
		accept_cleanup:
			close(client);
		accept_end:
			vTaskDelay(100 / portTICK_RATE_MS);
	}
}

#define HTTP_RESULT_CODE_OK 200
#define HTTP_ERROR_NOT_FOUND 404
#define HTTP_ERROR_METHOD_NOT_ALLOWED 405
#define HTTP_ERROR_BAD_REQUEST 300
#define HTTP_ERROR_VERSION_NOT_SUPPORTED 505

int parse_http(http_packet_t *packet, http_request_info_t *req_info) {
	*req_info = (http_request_info_t){};
	char* line_end = strstr(packet->buf, "\r\n");
    if (line_end == NULL) {
        os_printf("[%s] Error: Malformed request line (no CRLF).\n", __FUNCTION__);
		req_info->http_return_code = HTTP_ERROR_BAD_REQUEST;
        return 1;
    }

    // Calculate length of the request line
    size_t line_len = line_end - packet->buf;

    // Temporarily null-terminate the request line for parsing
    *line_end = '\0';

    char* first_space = strchr(packet->buf, ' ');
    if (first_space == NULL || first_space >= line_end) {
        os_printf("[%s] Error: Malformed request line (no space after method).\n", __FUNCTION__);
        *line_end = '\r';
		req_info->http_return_code = HTTP_ERROR_BAD_REQUEST;
        return 1;
    }
    size_t method_len = first_space - packet->buf;
    if (method_len >= 8) {
        os_printf("[%s] Error: Method too long.\n", __FUNCTION__);
        *line_end = '\r'; 
		req_info->http_return_code = HTTP_ERROR_BAD_REQUEST;
        return 1;
    }
    memcpy(req_info->method, packet->buf, method_len);
    req_info->method[method_len] = '\0';

    char* path_start = first_space + 1;
    char* second_space = strchr(path_start, ' ');
    if (second_space == NULL || second_space >= line_end) {
        os_printf("[%s] Error: Malformed request line (no space after path).\n", __FUNCTION__);
        *line_end = '\r'; 
		req_info->http_return_code = HTTP_ERROR_BAD_REQUEST;
        return 1;
    }
    size_t path_len = second_space - path_start;
	if (path_len >= 8) {
        os_printf("[%s] Error: Path too long.\n", __FUNCTION__);
        *line_end = '\r';
		req_info->http_return_code = HTTP_ERROR_BAD_REQUEST;
        return 1;
    }
    memcpy(req_info->path, path_start, path_len);
    req_info->path[path_len] = '\0';

	char* version_start = second_space + 1;
	size_t version_len = line_end - version_start; 
	if (version_len >= 10) {
        os_printf("[%s] Error: Version string too long.\n", __FUNCTION__);
        *line_end = '\r'; 
		req_info->http_return_code = HTTP_ERROR_BAD_REQUEST;
        return 1;
    }
    memcpy(req_info->version, version_start, version_len);
    req_info->version[version_len] = '\0';

    // Restore the original character at line_end
    *line_end = '\r';

    os_printf("[%s] Parsed Request: Method='%s', Path='%s', Version='%s'\n", __FUNCTION__, req_info->method, req_info->path, req_info->version);

	if (strcmp(req_info->method, "GET")) {
		os_printf("[%s] Cannot handle %s method\n", __FUNCTION__, req_info->method);
		req_info->http_return_code = HTTP_ERROR_METHOD_NOT_ALLOWED;
		return 2;
	}
	if (strcmp(req_info->path, "/")) {
		os_printf("[%s] Cannot handle path %s\n", __FUNCTION__, req_info->path);
		req_info->http_return_code = HTTP_ERROR_NOT_FOUND;
		return 3;
	}
	// Handle only HTTP/1.1 requests
	if (strcmp(req_info->version, "HTTP/1.1")) {
		os_printf("[%s] Cannot handle HTTP version %s\n", __FUNCTION__, req_info->version);
		req_info->http_return_code = HTTP_ERROR_VERSION_NOT_SUPPORTED;
		return 4;
	}
	req_info->http_return_code = HTTP_RESULT_CODE_OK;
    return 0;
}

void client_task(void *arg) {
	fd_set read_fds, write_fds, except_fds;
    struct timeval timeout = {
		.tv_sec = 0,
		.tv_usec = 100000,
	};
	int parse_http_result = 0;

	while(1) {
		os_printf("[%s] Max bytes used: %d\n", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL));
		os_printf("[%s] Heap bytes remaining: %u\n", __FUNCTION__, xPortGetFreeHeapSize());
		
		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);
		FD_ZERO(&except_fds);
		processed_client_socks_len = 0;
		
        int max_fd = 0; // Find the maximum file descriptor number

        // Protect access to the shared list of client sockets
        if (xSemaphoreTake(client_list_mutex, portMAX_DELAY) != pdTRUE) {
			os_printf("[%s] %d FATAL: Failed to take client_list_mutex.\n", __FUNCTION__, __LINE__);
			while (1) vTaskDelay(1000);
		}
        // Populate the fd_sets and find max_fd
        for (int i = 0; i < num_clients; ++i) {
            int client_fd = active_ptr[i];
            if (client_fd >= 0) { // Ensure descriptor is valid
                FD_SET(client_fd, &read_fds);    // Monitor for reading (data or closure)
                FD_SET(client_fd, &except_fds);   // Monitor for errors/exceptions
                FD_SET(client_fd, &write_fds); // Monitor for writing
                if (client_fd > max_fd) {
                    max_fd = client_fd;
                }
            }
        }

		if (max_fd == 0 || num_clients == 0) {
			os_printf("[%s]: No active clients, delaying.\n", __FUNCTION__);
			if (xSemaphoreGive(client_list_mutex) != pdTRUE) {
				os_printf("[%s] %d FATAL: Failed to give client_list_mutex.\n", __FUNCTION__, __LINE__);
				while (1) vTaskDelay(1000);
			}
			vTaskDelay(2000 / portTICK_RATE_MS); // Delay if no clients to monitor
			continue; // Go to next iteration
		}

		int activity = lwip_select(max_fd + 1, &read_fds, &write_fds, &except_fds, &timeout);
        if (activity < 0) {
            os_printf("[%s] Select error %d: %s\n", __FUNCTION__, errno, strerror(errno));
			if (xSemaphoreGive(client_list_mutex) != pdTRUE) {
				os_printf("[%s] %d FATAL: Failed to give client_list_mutex.\n", __FUNCTION__, __LINE__);
				while (1) vTaskDelay(1000);
			}
			vTaskDelay(2000 / portTICK_RATE_MS);
            continue;
        }

        if (activity == 0) {
            // Timeout occurred, no activity.
            os_printf("[%s] Select timeout, no activity.\n", __FUNCTION__);
			if (xSemaphoreGive(client_list_mutex) != pdTRUE) {
				os_printf("[%s] %d FATAL: Failed to give client_list_mutex.\n", __FUNCTION__, __LINE__);
				while (1) vTaskDelay(1000);
			}
			vTaskDelay(2000 / portTICK_RATE_MS);
            continue;
        }

		for (int i = 0; i < num_clients; ++i) {
            int curr_client = active_ptr[i];
			packet = (http_packet_t){};
            // Check for error(s)
            if (FD_ISSET(curr_client, &except_fds)) {
				os_printf("[%s] Client socket %d detected exceptional condition. Closing.\n", __FUNCTION__, curr_client);
				close(curr_client);
				if (xSemaphoreGive(accept_semaphore) != pdTRUE) {
					os_printf("[%s] %d FATAL: Failed to give accept_semaphore.\n", __FUNCTION__, __LINE__);
					while (1) vTaskDelay(1000);
				}
				goto client_end;
            }

            // Only check read_fds if the socket didn't err
            if (FD_ISSET(curr_client, &read_fds)) {
				// Attempt to read data
				os_printf("[%s] Reading from client %d\n",__FUNCTION__, curr_client);
				char buf[1024];
				bool header_complete = false;

				while (1) {
					int bytes_received = recv(curr_client, buf, sizeof(buf), 0);
					if (bytes_received == sizeof(buf)) {
						os_printf("[%s] Buffer size too small.\n", __FUNCTION__);
						goto client_end;
					}
					os_printf("[%s] bytes received: %d byte(s)\n", __FUNCTION__, bytes_received);
					
					if (bytes_received < 0) {
						os_printf("[%s] ERROR: %s (%d)\n", __FUNCTION__, strerror(errno), errno);
						if (errno != EWOULDBLOCK) {
							// Real error during recv (connection broken)
							os_printf("[%s] Recv error on client %d. Closing. Return code: %d.\n", curr_client, bytes_received);
							close(curr_client);
							if (xSemaphoreGive(accept_semaphore) != pdTRUE) {
								os_printf("[%s] %d FATAL: Failed to give accept_semaphore\n", __FUNCTION__, __LINE__);
								while (1) vTaskDelay(1000);
							}
							goto client_end;
						}
						goto client_add;
					} else if (bytes_received == 0) {
						// Client closed the connection
						os_printf("Client socket %d closed gracefully by client. Closing local socket.\n", curr_client);
						close(curr_client);
						if (xSemaphoreGive(accept_semaphore) != pdTRUE) {
							os_printf("[%s] %d FATAL: Failed to give accept_semaphore\n", __FUNCTION__, __LINE__);
								while (1) vTaskDelay(1000);
						}
						goto client_end;
					} else {
						buf[bytes_received] = 0;
						os_printf("[%s] req:\n--START--\n%s--END--\n", __FUNCTION__, buf);
						strncpy(packet.buf, buf, bytes_received);
						packet.len += bytes_received;
						if (strstr(buf, HEADER_END) != NULL) {
							header_complete = true;
							break; // Header is complete
						}
						if (bytes_received >= MAX_PACKET_SIZE) break;
					}
				}
				if (!header_complete) {
					os_printf("Error: Header not complete or too large for buffer %d.\n", curr_client);
					goto client_add; // Header too large or missing end marker
				}
				if (parse_http(&packet, &req_info)) {
					os_printf("[%s] Failed to parse packet\n", __FUNCTION__);
					vTaskDelay(100 / portTICK_RATE_MS);
				}
            }

            if (FD_ISSET(curr_client, &write_fds) && packet.len) {
				send_data(curr_client, &req_info);
			}
			client_add:
				processed_ptr[processed_client_socks_len++] = curr_client;

            // If any error occurred, skip copying the socket before switching the buffer.
			client_end:
				continue;
        } // End for loop processing revents
		int *temp = processed_ptr;
		num_clients = processed_client_socks_len;
		processed_ptr = active_ptr;
		active_ptr = temp;
		os_printf("[%s] releasing client_list_mutex.\n", __FUNCTION__);
		if (xSemaphoreGive(client_list_mutex) != pdTRUE) {
			os_printf("[%s] %d FATAL: Failed to give client_list_mutex.\n", __FUNCTION__, __LINE__);
			while (1) vTaskDelay(1000);
		}
		vTaskDelay(2000 / portTICK_RATE_MS);
	}
}

int32_t send_data(int client_socket, http_request_info_t *req_info) {
	char response[1024];
	char body[512]; // Make sure this is large enough for your body
	int total_len = 0;
	os_printf("[%s] req_info->http_return_code: %d, count: %d\n", __FUNCTION__, req_info->http_return_code);
	switch (req_info->http_return_code) {
		case HTTP_RESULT_CODE_OK: {
			int count = get_count();
			int body_len = snprintf(body, sizeof(body),
								"<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Exercise Result</title></head><body><h1>Results</h1><p>You did %d push-up(s). Congratulations!!</p></body></html>",
								count);
	
			if (body_len < 0 || body_len >= sizeof(body)) {
				os_printf("Error: Could not format response body or buffer too small.\n");
				return -1;
			}
			int header_len = snprintf(response, sizeof(response),
								"HTTP/1.1 200 OK\r\n"
								"Content-Type: text/html; charset=utf-8\r\n"
								"Content-Length: %d\r\n" // Use the calculated body_len
								"Connection: keep-alive\r\n"
								"\r\n",
								body_len);
			if (header_len < 0 || header_len >= sizeof(response)) {
				os_printf("Error: Could not format response header or buffer too small.\n");
				return -1;
			}
			total_len = header_len + body_len;
			if (total_len >= sizeof(response)) {
				os_printf("Error: Combined header and body too large for response buffer.\n");
				return -1;
			}
			memcpy(response + header_len, body, body_len); // Use memcpy since body is already formatted
			response[total_len] = '\0';
		} break;
		case HTTP_ERROR_METHOD_NOT_ALLOWED: {
			int body_len = snprintf(body, sizeof(body),
								"<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>405 Method Not Allowed</title></head><body>Requested Method %s Not Allowed</body></html>", 
								req_info->method);
			if (body_len < 0 || body_len >= sizeof(body)) {
				os_printf("Error: Could not format response body or buffer too small.\n");
				return -1;
			}
			int header_len = snprintf(response, sizeof(response),
								"HTTP/1.1 405 Method Not Allowed\r\n"
								"Content-Type: text/html; charset=utf-8\r\n"
								"Content-Length: %d\r\n" // Use the calculated body_len
								"Connection: keep-alive\r\n"
								"\r\n",
								body_len);
			if (header_len < 0 || header_len >= sizeof(response)) {
				os_printf("Error: Could not format response header or buffer too small.\n");
				return -1;
			}
			total_len = header_len + body_len;
			if (total_len >= sizeof(response)) {
				os_printf("Error: Combined header and body too large for response buffer.\n");
				return -1;
			}
			memcpy(response + header_len, body, body_len); // Use memcpy since body is already formatted
			response[total_len] = '\0';
		} break;
		case HTTP_ERROR_VERSION_NOT_SUPPORTED: {
			int body_len = snprintf(body, sizeof(body),
								"<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>505 Version Not Supported</title></head><body>HTTP Version %s Not Supported</body></html>", 
								req_info->version);
			if (body_len < 0 || body_len >= sizeof(body)) {
				os_printf("Error: Could not format response body or buffer too small.\n");
				return -1;
			}
			int header_len = snprintf(response, sizeof(response),
								"HTTP/1.1 505 Version Not Supported\r\n"
								"Content-Type: text/html; charset=utf-8\r\n"
								"Content-Length: %d\r\n" // Use the calculated body_len
								"Connection: keep-alive\r\n"
								"\r\n",
								body_len);
			if (header_len < 0 || header_len >= sizeof(response)) {
				os_printf("Error: Could not format response header or buffer too small.\n");
				return -1;
			}
			total_len = header_len + body_len;
			if (total_len >= sizeof(response)) {
				os_printf("Error: Combined header and body too large for response buffer.\n");
				return -1;
			}
			memcpy(response + header_len, body, body_len); // Use memcpy since body is already formatted
			response[total_len] = '\0';
		} break;
		case HTTP_ERROR_NOT_FOUND: {
			int body_len = snprintf(body, sizeof(body),
								"<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>404 Not Found</title></head><body>Requested URL %s Not Found</body></html>", 
								req_info->path);
			if (body_len < 0 || body_len >= sizeof(body)) {
				os_printf("Error: Could not format response body or buffer too small.\n");
				return -1;
			}
			int header_len = snprintf(response, sizeof(response),
								"HTTP/1.1 404 Not Found\r\n"
								"Content-Type: text/html; charset=utf-8\r\n"
								"Content-Length: %d\r\n" // Use the calculated body_len
								"Connection: keep-alive\r\n"
								"\r\n",
								body_len);
			if (header_len < 0 || header_len >= sizeof(response)) {
				os_printf("Error: Could not format response header or buffer too small.\n");
				return -1;
			}
			total_len = header_len + body_len;
			if (total_len >= sizeof(response)) {
				os_printf("Error: Combined header and body too large for response buffer.\n");
				return -1;
			}
			memcpy(response + header_len, body, body_len); // Use memcpy since body is already formatted
			response[total_len] = '\0';
		} break;
		case HTTP_ERROR_BAD_REQUEST: {
			int body_len = snprintf(body, sizeof(body),
								"<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>300 Bad Request</title></head><body>Request cannot be handled</body></html>");
			if (body_len < 0 || body_len >= sizeof(body)) {
				os_printf("Error: Could not format response body or buffer too small.\n");
				return -1;
			}
			int header_len = snprintf(response, sizeof(response),
								"HTTP/1.1 300 Bad Request\r\n"
								"Content-Type: text/html; charset=utf-8\r\n"
								"Content-Length: %d\r\n" // Use the calculated body_len
								"Connection: keep-alive\r\n"
								"\r\n",
								body_len);
			if (header_len < 0 || header_len >= sizeof(response)) {
				os_printf("Error: Could not format response header or buffer too small.\n");
				return -1;
			}
			total_len = header_len + body_len;
			if (total_len >= sizeof(response)) {
				os_printf("Error: Combined header and body too large for response buffer.\n");
				return -1;
			}
			memcpy(response + header_len, body, body_len); // Use memcpy since body is already formatted
			response[total_len] = '\0';
		} break;
	}
	os_printf("response: %s\n", response);
	os_printf("[%s] total len: %d\n", __FUNCTION__ ,total_len);
	int bytes_sent = 0;
	while (bytes_sent < total_len) {
		int res = send(client_socket, response + bytes_sent, total_len - bytes_sent, 0);
		if (res < 0) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				os_printf("Client %d: send would block. Will retry later.\n", client_socket);
				return -EWOULDBLOCK; // Indicate incomplete send
			} else {
				os_printf("Error sending data to client %d, errno %d: %s\n", client_socket, errno, strerror(errno));
				return -errno; // Return negative error code
			}
		} else {
			bytes_sent += res;
			os_printf("Client %d: Sent %d bytes (total %d/%d)\n", client_socket, res, bytes_sent, total_len);
		}
	}
	os_printf("Client %d: Successfully sent complete response.\n", client_socket);
	return bytes_sent; 
}