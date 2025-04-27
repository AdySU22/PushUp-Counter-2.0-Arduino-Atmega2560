#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_common.h>
#include <gpio.h>
#include <freertos/projdefs.h>
#include <freertos/semphr.h> 

#include "client.h"

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
	os_printf("Heap bytes remaining: %u\n", xPortGetFreeHeapSize());
	os_printf("Wi-Fi init returned with code %d\n", init_server());
	os_printf("Heap bytes remaining: %u\n", xPortGetFreeHeapSize());
	portBASE_TYPE res = xTaskCreate(&scan_task, "wifi_scan_task", 256, NULL, 0, &wifi_scan_task_handle);
	if (res != pdPASS) {
		os_printf("Failed to create wifi_scan_task (0x%X).\n", res);
		while (1) {
			vTaskDelay(1);
		};
	}
	os_printf("Created wifi_scan_task\n");
	res = xTaskCreate(&accept_task, "wifi_accept_task", 256, NULL, 0, &wifi_accept_task_handle);
	if (res != pdPASS) {
		os_printf("Failed to create wifi_accept_task (0x%X).\n", res);
		while (1) {
			vTaskDelay(1);
		};
	}
	os_printf("Created wifi_accept_task\n");
	res = xTaskCreate(&client_task, "wifi_client_task", 2048, NULL, 1, &wifi_client_task_handle);
	if (res != pdPASS) {
		os_printf("Failed to create wifi_client_task (0x%X).\n", res);
		while (1) {
			vTaskDelay(1);
		};
	}
	os_printf("Created wifi_client_task\n");
	os_printf("Heap bytes remaining: %u\n", xPortGetFreeHeapSize());
	while (1) {
		os_printf("%s Bytes used: %d\n", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL));
		os_printf("Heap bytes remaining: %u\n", xPortGetFreeHeapSize());
		int rssi = wifi_station_get_rssi();
		if (rssi != 31) {
			selected_ap->rssi = rssi;
		}
		os_printf("Current wifi station rssi: %d\n", selected_ap->rssi);
		if (selected_ap->rssi < -35) {
			xSemaphoreGive(scan_semaphore);
			vTaskResume(wifi_scan_task_handle);
		} 
		if (num_clients >= MAX_CLIENT_NUM) {
			xSemaphoreGive(accept_semaphore);
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
			vTaskDelay(1);
		};
	}
	accept_semaphore = xSemaphoreCreateCounting(MAX_CLIENT_NUM, 0); 
	if (!accept_semaphore) { 
		os_printf("Failed to create accept_semaphore.\n");
		while (1) {
			vTaskDelay(1);
		};
	}
	client_list_mutex = xSemaphoreCreateMutex(); 
	if (!client_list_mutex) { 
		os_printf("Failed to create client_list_mutex.\n");
		while (1) {
			vTaskDelay(1);
		};
	}

	if (xTaskCreate(&wifi_task, "wifi_task", 512, NULL, 0, &wifi_task_handle) != pdPASS) {
		os_printf("Failed to create wifi_task.\n");
		while (1) {
			vTaskDelay(1);
		};
	}
	os_printf("Heap bytes remaining: %u\n", xPortGetFreeHeapSize());
	os_printf("Created wifi_task.\n");
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
//     gpio16_output_conf();
//     while(true) {
//     	gpio16_output_set(0);
//         vTaskDelay(1000/portTICK_RATE_MS);
//     	gpio16_output_set(1);
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