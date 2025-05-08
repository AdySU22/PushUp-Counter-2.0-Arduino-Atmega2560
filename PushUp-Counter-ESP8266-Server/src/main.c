#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_common.h>
#include <gpio.h>
#include <freertos/projdefs.h>
#include <freertos/semphr.h> 

#include "client.h"
#include "spi.h"

xSemaphoreHandle scan_semaphore = NULL; 
xSemaphoreHandle accept_semaphore = NULL;
xSemaphoreHandle client_list_mutex = NULL;
xTaskHandle wifi_task_handle = NULL, 
	wifi_scan_task_handle = NULL, 
	wifi_accept_task_handle = NULL,
	wifi_client_task_handle = NULL;

uint32_t user_rf_cal_sector_set(void) {
    flash_size_map size_map = system_get_flash_size_map();
    uint32_t rf_cal_sec = 0;
    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        default:
            rf_cal_sec = 0;
            break;
    }
    return rf_cal_sec;
}

void wifi_task(void *arg) {
	os_printf("[%s] Heap bytes remaining: %u\n", __FUNCTION__, xPortGetFreeHeapSize());
	portBASE_TYPE res = init_server(); 
	os_printf("Wi-Fi init returned with code %d\n", res);
	if (res) {
		while (1) vTaskDelay(1);
	}
	os_printf("[%s] Heap bytes remaining: %u\n", __FUNCTION__, xPortGetFreeHeapSize());
	// res = xTaskCreate(&scan_task, "wifi_scan_task", 256, NULL, 0, &wifi_scan_task_handle);
	// if (res != pdPASS) {
	// 	os_printf("Failed to create %s (0x%X).\n", "wifi_scan_task", res);
	// 	while (1) {
	// 		vTaskDelay(1);
	// 	};
	// }

	os_printf("Created %s\n", "wifi_scan_task");
	os_printf("[%s] Heap bytes remaining: %u\n", __FUNCTION__, xPortGetFreeHeapSize());
	res = xTaskCreate(&accept_task, "wifi_accept_task", 512, NULL, 0, &wifi_accept_task_handle);
	if (res != pdPASS) {
		os_printf("Failed to create %s (0x%X).\n", "wifi_accept_task", res);
		while (1) {
			vTaskDelay(1);
		};
	}
	
	os_printf("Created %s\n", "wifi_accept_task");
	os_printf("[%s] Heap bytes remaining: %u\n", __FUNCTION__, xPortGetFreeHeapSize());

	res = xTaskCreate(&client_task, "wifi_client_task", 1024, NULL, 1, &wifi_client_task_handle);
	if (res != pdPASS) {
		os_printf("Failed to create %s (0x%X).\n", "wifi_client_task", res);
		while (1) {
			vTaskDelay(1);
		};
	}

	os_printf("Created %s\n", "wifi_client_task");
	os_printf("[%s] Heap bytes remaining: %u\n", __FUNCTION__, xPortGetFreeHeapSize());
	while (1) {
		os_printf("%s Bytes used: %d\n", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL));
		os_printf("[%s] Heap bytes remaining: %u\n", __FUNCTION__, xPortGetFreeHeapSize());
		// os_printf("Current wifi station rssi: %d\n", selected_ap->rssi); 
		os_printf("Connection status: ");
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
				wifi_station_connect();
			} break;
			case STATION_CONNECT_FAIL: {
				os_printf("Failed to connect\n");
				// if (selected_ap->rssi < -35 || selected_ap->rssi == 31) {
				// 	xSemaphoreGive(scan_semaphore);
				// 	vTaskResume(wifi_scan_task_handle);
				// }
			} break;
			case STATION_GOT_IP: {
				os_printf("Got IP\n");
				int rssi = wifi_station_get_rssi();
				os_printf("rssi: %d\n", rssi);
				if (rssi != 31) {
					selected_ap->rssi = rssi;
				}
			} break;
		}
		print_station_clients();
		
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
}

void user_init(void) {
	#ifdef __SINGLE_THREAD__
	os_printf("Single threaded mode.\n");
	#endif
	vSemaphoreCreateBinary(scan_semaphore);
	if (!scan_semaphore) {
		os_printf("Failed to create scan_semaphore.\n");
		while (1) {
			gpio_output_conf(0x4, 0, 0xFFFF, 0);
			vTaskDelay(250/portTICK_RATE_MS);
			gpio_output_conf(0, 0x4, 0xFFFF, 0);
			vTaskDelay(250/portTICK_RATE_MS);
		};
	}
	accept_semaphore = xSemaphoreCreateCounting(MAX_CLIENT_NUM, MAX_CLIENT_NUM); 
	if (!accept_semaphore) { 
		os_printf("Failed to create accept_semaphore.\n");
		while (1) {
			gpio_output_conf(0x4, 0, 0xFFFF, 0);
			vTaskDelay(250/portTICK_RATE_MS);
			gpio_output_conf(0, 0x4, 0xFFFF, 0);
			vTaskDelay(250/portTICK_RATE_MS);
		};
	}
	client_list_mutex = xSemaphoreCreateMutex(); 
	if (!client_list_mutex) { 
		os_printf("Failed to create client_list_mutex.\n");
		while (1) {
			gpio_output_conf(0x4, 0, 0xFFFF, 0);
			vTaskDelay(250/portTICK_RATE_MS);
			gpio_output_conf(0, 0x4, 0xFFFF, 0);
			vTaskDelay(250/portTICK_RATE_MS);
		};
	}
	
	if (xTaskCreate(&wifi_task, "wifi_task", 512, NULL, 0, &wifi_task_handle) != pdPASS) {
		os_printf("Failed to create wifi_task.\n");
		while (1) {
			gpio_output_conf(0x4, 0, 0xFFFF, 0);
			vTaskDelay(250/portTICK_RATE_MS);
			gpio_output_conf(0, 0x4, 0xFFFF, 0);
			vTaskDelay(250/portTICK_RATE_MS);
		};
	}
	os_printf("[%s] Heap bytes remaining: %u\n", __FUNCTION__, xPortGetFreeHeapSize());
	os_printf("[%s] Created wifi_task.\n", __FUNCTION__);
	init_spi();
	if (xTaskCreate(&spi_task, "spi_task", 256, NULL, 0, NULL) != pdPASS) {
		os_printf("Failed to create spi_task.\n");
		while (1) {
			gpio_output_conf(0x4, 0, 0xFFFF, 0);
			vTaskDelay(250/portTICK_RATE_MS);
			gpio_output_conf(0, 0x4, 0xFFFF, 0);
			vTaskDelay(250/portTICK_RATE_MS);
		};
	}

	gpio_output_conf(0, 0x4, 0xFFFF, 0);
}


// #include "esp_common.h"
// #include "freertos/task.h"
// #include "gpio.h"

// uint32 user_rf_cal_sector_set(void)
// {
//     flash_size_map size_map = system_get_flash_size_map();
//     uint32 rf_cal_sec = 0;
//     switch (size_map) {
//         case FLASH_SIZE_4M_MAP_256_256:
//             rf_cal_sec = 128 - 5;
//             break;

//         case FLASH_SIZE_8M_MAP_512_512:
//             rf_cal_sec = 256 - 5;
//             break;

//         case FLASH_SIZE_16M_MAP_512_512:
//         case FLASH_SIZE_16M_MAP_1024_1024:
//             rf_cal_sec = 512 - 5;
//             break;

//         case FLASH_SIZE_32M_MAP_512_512:
//         case FLASH_SIZE_32M_MAP_1024_1024:
//             rf_cal_sec = 1024 - 5;
//             break;

//         default:
//             rf_cal_sec = 0;
//             break;
//     }

//     return rf_cal_sec;
// }

// void task_blink(void* ignore)
// {
//     gpio_output_conf(0, 0, 0xFFFF, 0);
//     while(true) {
//     	gpio_output_conf(0x4, 0, 0xFFFF, 0);
//         vTaskDelay(1000/portTICK_RATE_MS);
//     	gpio_output_conf(0, 0x4, 0xFFFF, 0);
//         vTaskDelay(1000/portTICK_RATE_MS);
//     }
//     vTaskDelete(NULL);
// }

// /******************************************************************************
//  * FunctionName : user_init
//  * Description  : entry of user application, init user function here
//  * Parameters   : none
//  * Returns      : none
// *******************************************************************************/
// void user_init(void)
// {
//     xTaskCreate(&task_blink, "startup", 2048, NULL, 1, NULL);
// }