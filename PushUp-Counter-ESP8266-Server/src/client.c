#include "client.h"

extern xSemaphoreHandle scan_semaphore; 
extern xSemaphoreHandle accept_semaphore;
extern xSemaphoreHandle client_list_mutex;
bool use_open_wifi = false;
conn_config_t *selected_ap = NULL;

#define HEADER_END "\r\n\r\n"

static int server_sock, active_client_socks[MAX_CLIENT_NUM], processed_client_socks[MAX_CLIENT_NUM], processed_client_socks_len;
int *active_ptr = &active_client_socks[0], *processed_ptr = &processed_client_socks[0];
int num_clients;

void set_use_open_wifi(bool use) {
	use_open_wifi = use;
}

void scan_callback(void *arg, STATUS status) {
	os_printf("Scan status: ");
	switch (status) {
		case OK: {
			os_printf("OK\n");
			bss_info_t *bss_info = (bss_info_t*)arg;
			while (bss_info) {
				os_printf("BSSID / MAC: %d:%d:%d:%d:%d:%d\nSSID: %s\nChannel: %d\nRSSI: %d\nAuth Mode: ", 
					bss_info->bssid[0], bss_info->bssid[1], bss_info->bssid[2], bss_info->bssid[3], bss_info->bssid[4], bss_info->bssid[5],
					bss_info->ssid, bss_info->channel, bss_info->rssi
				);
				switch (bss_info->authmode) {
					case AUTH_OPEN: {
						os_printf("OPEN\n");
						if (use_open_wifi && bss_info->rssi >= selected_ap->rssi) {
							selected_ap->sta_conf.bssid_set = 0;
							selected_ap->rssi = bss_info->rssi;
							memcpy(selected_ap->sta_conf.ssid, bss_info->ssid, strlen(bss_info->ssid));
						}
						os_printf("Connected to %s: %s\n", selected_ap->sta_conf.ssid, wifi_station_connect() ? "True" : "False");
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
				os_printf("Hidden: %s\nFrequency Offset: %d\nFrequency Calibration: %d\n\n", bss_info->is_hidden ? "True" : "False", bss_info->freq_offset, bss_info->freqcal_val);
				bss_info = bss_info->next.stqe_next;
			}
		} break;
		case PENDING: {
			os_printf("Pending\n");
		}	break;
		case FAIL: {
			os_printf("Failed\n");
		} break;
		case BUSY: {
			os_printf("Busy\n");
		} break;
		case CANCEL: {
			os_printf("Cancelled\n");
		} break;
		default:
			break;
	}
	vTaskDelay(100 / portTICK_RATE_MS);
}

int32_t init_server(void) {
	if (!wifi_set_opmode_current(STATIONAP_MODE)) return 1;
	sta_config_t sta_conf = {
		.password = STA_PASS,
		.ssid = STA_SSID
	};
	selected_ap = os_malloc(sizeof(conn_config_t));
	*selected_ap = (conn_config_t){
		.sta_conf = sta_conf,
		.rssi = 31
	};
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
	if (!wifi_set_channel(CHANNEL)) return 3;
	scan_config_t scan_conf = {
		.bssid = NULL,
		.channel = CHANNEL,
		.show_hidden = true,
	};
	wifi_station_scan(&scan_conf, scan_callback);
	dchp_status_t dhcp = wifi_softap_dhcps_status();
	os_printf("dhcp status: %s\n", dhcp == DHCP_STARTED ? "Started" : "Stopped");
	os_printf("WiFi AP SSID: %s\nWifi Password: %s\n", AP_SSID, AP_PASS);
	if (!wifi_station_set_config(&sta_conf)) {
		return 4;
	}
	os_printf("Connecting to %s\n", STA_SSID);
	if (!wifi_station_connect()) {
		os_printf("Failed to connect to %s\n", STA_SSID);
		return 5;
	}
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
	return init_sockets();
}

void print_station_clients(void) {
	os_printf("Current clients:\n");
	sta_info_t *sta_info =  wifi_softap_get_station_info();
	if (!sta_info) {
		os_printf("No clients.\n"); 
		return;
	}
	sta_info_t *curr = sta_info;
	while (curr) {
		os_printf("BSSID / MAC: %d:%d:%d:%d:%d:%d, \nIP: %d.%d.%d.%d\n", curr->bssid[0], 
			curr->bssid[1], curr->bssid[2], curr->bssid[3], curr->bssid[4], curr->bssid[5],
			((curr->ip.addr >> 24) & 0xFF), ((curr->ip.addr >> 16) & 0xFF), ((curr->ip.addr >> 8) & 0xFF), (curr->ip.addr & 0xFF));
		curr = curr->next.stqe_next;
	}
	wifi_softap_free_station_info();
}

int32_t send_data(uint16_t data) {
	
}

int32_t init_sockets(void) {
	lwip_socket_init();
	int res = 0;
	int option = 1;
	server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int curr_errno = errno;
	if (server_sock < 0) {
		printf("Error creating server socket: %s\n", strerror(curr_errno));
		return curr_errno;	
	}
	
	res = setsockopt(server_sock, SOL_SOCKET, SO_KEEPALIVE, &option, sizeof(option));
	curr_errno = errno;
	if (res < 0) {
		os_printf("Error setting SO_KEEPALIVE option for server socket: %s\n", strerror(curr_errno));
		return curr_errno;
	}
	
	int keep_idle = 30; // Time in seconds until the first probe on an idle connection
	res = setsockopt(server_sock, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
	curr_errno = errno;
	if (res < 0) {
		os_printf("Error setting TCP_KEEPIDLE option for server socket: %s\n", strerror(curr_errno));
		return curr_errno;
	} 
	
	// Set interval between probes
	int keep_intvl = 5; // Time in seconds between subsequent probes
	res = setsockopt(server_sock, IPPROTO_TCP, TCP_KEEPINTVL, &keep_intvl, sizeof(keep_intvl));
	curr_errno = errno;
	if (res < 0) {
		os_printf("Error setting TCP_KEEPINTVL option for server socket: %s\n", strerror(curr_errno));
		return curr_errno;
	}
	
	// Set number of probes before dropping connection
	int keep_cnt = 3; // Number of probes before connection is considered dead
	res = setsockopt(server_sock, IPPROTO_TCP, TCP_KEEPCNT, &keep_cnt, sizeof(keep_cnt));
	curr_errno = errno;
	if (res < 0) {
		os_printf("Error setting TCP_KEEPCNT option for server socket: %s\n", strerror(curr_errno));
		return curr_errno;
	}
	
	int flags = fcntl(server_sock, F_GETFL, 0);
	curr_errno = errno;
	if (flags < 0) {
		os_printf("Error getting socket flags: %s\n", strerror(curr_errno));
		return curr_errno;
	}
	
	// Set non-blocking flag
	flags |= O_NONBLOCK; // Add O_NONBLOCK flag
	
	if (fcntl(server_sock, F_SETFL, flags) < 0) {
		curr_errno = errno;
		os_printf("Error setting socket flags: %s\n", strerror(curr_errno));
		return curr_errno;
	}
	
	struct sockaddr_in address = {
		.sin_len = sizeof(struct sockaddr_in),
		.sin_family  = AF_INET,
		.sin_addr = {
			.s_addr = INADDR_ANY
		},
		.sin_port = htons(0)
	};

	res = bind(server_sock, (struct sockaddr*)&address, sizeof(struct sockaddr));
	curr_errno = errno;
	if (res < 0) {
		os_printf("Error listening to %d: %s\n", 0, strerror(curr_errno));
		return curr_errno;
	}

	struct sockaddr_in bound_address;
    socklen_t bound_address_len = sizeof(bound_address);
	res = lwip_getsockname(server_sock, (struct sockaddr *)&bound_address, &bound_address_len);
	curr_errno = errno;
	if (res < 0) {
		os_printf("Failed to get server's socket address: %s\n", strerror(curr_errno));
		return curr_errno;
	}

	res = listen(server_sock, 4);
	curr_errno = errno;
	int8_t ip_address[4] = {
		((bound_address.sin_addr.s_addr >> 24) & 0xFF), 
		((bound_address.sin_addr.s_addr >> 16) & 0xFF), 
		((bound_address.sin_addr.s_addr >> 8) & 0xFF), 
		bound_address.sin_addr.s_addr & 0xFF
	};
	if (res < 0) {
		os_printf("Error listening to %d.%d.%d.%d:%d: %s\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3], ntohs(bound_address.sin_port), strerror(curr_errno));
		return curr_errno;
	}
	os_printf("Server listening at %d.%d.%d.%d:%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3], ntohs(bound_address.sin_port));
	return 0;
}

void scan_task(void *arg) {
	while (1) {
		if (xSemaphoreTake(scan_semaphore, portMAX_DELAY) != pdTRUE) {
			// This should not fail with portMAX_DELAY unless scheduler is stopped or heap corruption
			os_printf("FATAL: scan_task failed to take semaphore.\n");
			while(1) { vTaskDelay(1000); } // Halt
		}
		os_printf("%s Bytes used: %d\n", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL));
		os_printf("Heap bytes remaining: %u\n", xPortGetFreeHeapSize());
		os_printf("Connected to %s\n", selected_ap->sta_conf.ssid);
		os_printf("Connection Status: ");
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
			os_printf("FATAL: accept_task failed to take semaphore.\n");
			while(1) { vTaskDelay(1000); } // Halt
		}
		if (xSemaphoreTake(client_list_mutex, portMAX_DELAY) != pdTRUE) {
			os_printf("FATAL: accept_task failed to take mutex.\n");
			while(1) { vTaskDelay(1000); } // Halt
		}
		int client = accept(server_sock, NULL, NULL);
		int curr_errno = errno;
		if (client < 0) {
			if (curr_errno != EWOULDBLOCK) {
				os_printf("Failed to accept client due to %d: %s\n", curr_errno, strerror(curr_errno));
			} else {
				os_printf("Call to accept() would block.\n");
			}
			goto accept_end;
		} else if (num_clients < MAX_CLIENT_NUM) {
			os_printf("Accepted client.");
			int option = 1;
			int res = setsockopt(client, SOL_SOCKET, SO_KEEPALIVE, &option, sizeof(option));
			curr_errno = errno;
			if (res < 0) {
				os_printf("Error setting SO_KEEPALIVE option for client socket %d: %s\n", num_clients, strerror(curr_errno));
				goto accept_cleanup;
			} 
			int keep_idle = 30; // Time in seconds until the first probe on an idle connection
			res = setsockopt(client, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
			curr_errno = errno;
			if (res < 0) {
				os_printf("Error setting TCP_KEEPIDLE option for server socket: %s\n", strerror(curr_errno));
				goto accept_cleanup;
			} 
			
			// Set interval between probes
			int keep_intvl = 5; // Time in seconds between subsequent probes
			res = setsockopt(client, IPPROTO_TCP, TCP_KEEPINTVL, &keep_intvl, sizeof(keep_intvl));
			curr_errno = errno;
			if (res < 0) {
				os_printf("Error setting TCP_KEEPINTVL option for server socket: %s\n", strerror(curr_errno));
				goto accept_cleanup;
			}
			
			// Set number of probes before dropping connection
			int keep_cnt = 3; // Number of probes before connection is considered dead
			res = setsockopt(client, IPPROTO_TCP, TCP_KEEPCNT, &keep_cnt, sizeof(keep_cnt));
			curr_errno = errno;
			if (res < 0) {
				os_printf("Error setting TCP_KEEPCNT option for server socket: %s\n", strerror(curr_errno));
				goto accept_cleanup;
			}
			
			int flags = fcntl(client, F_GETFL, 0);
			curr_errno = errno;
			if (flags < 0) {
				os_printf("Error getting socket flags: %s\n", strerror(curr_errno));
				goto accept_cleanup;
			}
			
			// Set non-blocking flag
			flags |= O_NONBLOCK; // Add O_NONBLOCK flag
			
			if (fcntl(client, F_SETFL, flags) < 0) {
				curr_errno = errno;
				os_printf("Error setting socket flags: %s\n", strerror(curr_errno));
				goto accept_cleanup;
			}
			os_printf("Added client\nNumber of clients: %d\n", num_clients);
			active_ptr[num_clients++] = client;
			goto accept_end;
		}
		accept_cleanup:
			close(client);
		accept_end:
			xSemaphoreGive(client_list_mutex);
			vTaskDelay(1000 / portTICK_RATE_MS);
	}
}

void client_task(void *arg) {
	fd_set read_fds, write_fds, except_fds;
    struct timeval timeout = {
		.tv_sec = 0,
		.tv_usec = 100000,
	};

	while(1) 
	{
		os_printf("%s Bytes used: %d\n", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL));
		os_printf("Heap bytes remaining: %u\n", xPortGetFreeHeapSize());
		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);
		FD_ZERO(&except_fds);
		processed_client_socks_len = 0;

        int max_fd = 0; // Find the maximum file descriptor number

        // Protect access to the shared list of client sockets
        xSemaphoreTake(client_list_mutex, portMAX_DELAY); 
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
			os_printf("Select task: No active clients, delaying.\n");
			vTaskDelay(100 / portTICK_RATE_MS); // Delay if no clients to monitor
			continue; // Go to next iteration
		}

		int activity = lwip_select(max_fd + 1, &read_fds, &write_fds, &except_fds, &timeout);
		int curr_errno = errno;
        if (activity < 0) {
            os_printf("Select error %d: %s\n", curr_errno, strerror(errno)); // Use appropriate logging
            vTaskDelay(100 / portTICK_RATE_MS);
            continue;
        }

        if (activity == 0) {
            // Timeout occurred, no activity. Just loop and call select again.
            os_printf("Select timeout, no activity.\n");
            continue;
        }

		for (int i = 0; i < num_clients; ) {
            int curr_client = active_ptr[i];
            // Check for error(s)
            if (FD_ISSET(curr_client, &except_fds)) {
				os_printf("Client socket %d detected exceptional condition. Closing.\n", curr_client);
				close(curr_client);
				xSemaphoreTake(accept_semaphore, portMAX_DELAY);
				goto client_end;
            }

            // Only check read_fds if the socket didn't err
            if (FD_ISSET(curr_client, &read_fds)) {
				// Attempt to read data
				os_printf("Reading from client %d\n", curr_client);
				char buffer[1024];
				int bytes_received = recv(curr_client, buffer, sizeof(buffer), 0);
				int total_bytes_received = 0;
				bool header_complete = false;

				while (total_bytes_received < sizeof(buffer) - 1) {
					if (bytes_received < 0) {
						int recv_errno = errno; // Capture errno if possible
						if (recv_errno != EWOULDBLOCK) {
							// Real error during recv (connection broken)
							// os_printf("Recv error on client %d: %d (%s). Closing.\n", curr_client, recv_errno, strerror(recv_errno)); // Use logging
							os_printf("Recv error on client %d. Closing. Return code: %d.\n", curr_client, bytes_received); // If errno broken
							// --- Close and Cleanup ---
							close(curr_client);
							xSemaphoreTake(accept_semaphore, portMAX_DELAY);
							goto client_end;
						}
					} else if (bytes_received == 0) {
						// Client closed the connection
						os_printf("Client socket %d closed gracefully by client. Closing local socket.\n", curr_client);
						close(curr_client);
						xSemaphoreTake(accept_semaphore, portMAX_DELAY);
						goto client_end;
					} else {
						total_bytes_received += bytes_received;
						buffer[total_bytes_received] = '\0';
						if (strstr(buffer, HEADER_END) != NULL) {
							header_complete = true;
							break; // Header is complete
						}
						 // --- Data Received! ---
						 // Process the received data (e.g., parse HTTP request)
						 // >>> This is where your HTTP Parsing & URL Routing Happens <<<
						 // Call your HTTP parsing function on 'buffer'
						 // Extract the URL path
						 // Call the appropriate request handler function based on the path
						 // handler_function(curr_client, parsed_request_info);
						 // Remember a single recv might not get the whole request.
					}
				}

				if (!header_complete) {
					os_printf("Error: Header not complete or too large for buffer %d.\n", curr_client);
					goto client_end; // Header too large or missing end marker
				}
			
				// --- Parse the Request Line (First Line) ---
				// Find the end of the first line (\r\n)
				char* first_line_end = strstr(buffer, "\r\n");
				if (first_line_end == NULL) {
					os_printf("Error: Malformed request, no CRLF in first line %d.\n", curr_client);
					goto client_end;
				}

				*first_line_end = '\0'; // Null-terminate the first line temporarily

				char method[10]; // Buffer for method (GET, POST, etc.)
				char path[128];  // Buffer for path (/, /data, etc.)

				char* first_space = strchr(buffer, ' ');
				if (first_space == NULL) {
					os_printf("Error: Malformed request line (no space) %d.\n", curr_client);
					goto client_end;
				}
				*first_space = '\0'; // Null-terminate method
				os_strncpy(method, buffer, sizeof(method) - 1);
				method[sizeof(method) - 1] = '\0'; // Ensure null termination

				char* second_space = strchr(first_space + 1, ' ');
				if (second_space == NULL) {
					os_printf("Error: Malformed request line (no second space) %d.\n", curr_client);
					goto client_end;
				}
				*second_space = '\0'; // Null-terminate path
				os_strncpy(path, first_space + 1, sizeof(path) - 1);
				path[sizeof(path) - 1] = '\0';
            }

            if (FD_ISSET(curr_client, &write_fds)) {
				// --- Handle Outgoing Data ---
				// You can now send data to curr_client without blocking
				// os_printf("Client socket %d ready for sending.\n", curr_client);
				// Check if you have pending data to send for this client
				// If yes, call send() (or write())
			}
			processed_ptr[processed_client_socks_len++] = curr_client;

            // If any error occurred, skip copying the socket before switching the buffer.
			client_end:
        } // End for loop processing revents
		int *temp = processed_ptr;
		num_clients = processed_client_socks_len;
		processed_ptr = active_ptr;
		active_ptr = temp;

		xSemaphoreGive(client_list_mutex);
	}
}