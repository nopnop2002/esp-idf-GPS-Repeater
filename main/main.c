/* GPS NMEA Repeater

	 This example code is in the Public Domain (or CC0 licensed, at your option.)

	 Unless required by applicable law or agreed to in writing, this
	 software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	 CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_vfs.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "lwip/dns.h"

#include "cmd.h"

static const char *TAG = "MAIN";

QueueHandle_t xQueueCmd;
QueueHandle_t xQueueSend;

#if CONFIG_STA_MODE
/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#endif

static void wifi_event_handler(void* arg, esp_event_base_t event_base, 
								int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT) ESP_LOGI(TAG, "WIFI_EVENT event_id=%d", event_id);
	if (event_base == IP_EVENT) ESP_LOGI(TAG, "IP_EVENT event_id=%d", event_id);

#if CONFIG_AP_MODE
	if (event_id == WIFI_EVENT_AP_STACONNECTED) {
		wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
		ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
	} else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
		wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
		ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
	}
#endif

#if CONFIG_STA_MODE
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		} else {
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		ESP_LOGI(TAG,"connect to the AP fail");
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
#endif
}

#if CONFIG_AP_MODE
void wifi_init_softap()
{
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_ap();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
									ESP_EVENT_ANY_ID,
									&wifi_event_handler,
									NULL,
									NULL));

	wifi_config_t wifi_config = {
		.ap = {
			.ssid = CONFIG_ESP_WIFI_SSID,
			.ssid_len = strlen(CONFIG_ESP_WIFI_SSID),
			.password = CONFIG_ESP_WIFI_PASSWORD,
			.max_connection = CONFIG_ESP_MAX_STA_CONN,
			.authmode = WIFI_AUTH_WPA_WPA2_PSK,
			.pmf_cfg = {
				.required = false,
			},
		},
	};
	if (strlen(CONFIG_ESP_WIFI_PASSWORD) == 0) {
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
}
#endif // CONFIG_AP_MODE

#if CONFIG_STA_MODE
void wifi_init_sta()
{
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_t *netif = esp_netif_create_default_wifi_sta();
	assert(netif);

#if CONFIG_STATIC_IP

	ESP_LOGI(TAG, "CONFIG_STATIC_IP_ADDRESS=[%s]",CONFIG_STATIC_IP_ADDRESS);
	ESP_LOGI(TAG, "CONFIG_STATIC_GW_ADDRESS=[%s]",CONFIG_STATIC_GW_ADDRESS);
	ESP_LOGI(TAG, "CONFIG_STATIC_NM_ADDRESS=[%s]",CONFIG_STATIC_NM_ADDRESS);

	/* Stop DHCP client */
	ESP_ERROR_CHECK(esp_netif_dhcpc_stop(netif));
	ESP_LOGI(TAG, "Stop DHCP Services");

	/* Set STATIC IP Address */
	esp_netif_ip_info_t ip_info;
	memset(&ip_info, 0 , sizeof(esp_netif_ip_info_t));
	ip_info.ip.addr = ipaddr_addr(CONFIG_STATIC_IP_ADDRESS);
	ip_info.netmask.addr = ipaddr_addr(CONFIG_STATIC_NM_ADDRESS);
	ip_info.gw.addr = ipaddr_addr(CONFIG_STATIC_GW_ADDRESS);;
	esp_netif_set_ip_info(netif, &ip_info);

	/*
	I referred from here.
	https://www.esp32.com/viewtopic.php?t=5380
	if we should not be using DHCP (for example we are using static IP addresses),
	then we need to instruct the ESP32 of the locations of the DNS servers manually.
	Google publicly makes available two name servers with the addresses of 8.8.8.8 and 8.8.4.4.
	*/

	ip_addr_t d;
	d.type = IPADDR_TYPE_V4;
	d.u_addr.ip4.addr = 0x08080808; //8.8.8.8 dns
	dns_setserver(0, &d);
	d.u_addr.ip4.addr = 0x08080404; //8.8.4.4 dns
	dns_setserver(1, &d);

#endif // CONFIG_STATIC_IP

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
									ESP_EVENT_ANY_ID,
									&wifi_event_handler,
									NULL,
									&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
									IP_EVENT_STA_GOT_IP,
									&wifi_event_handler,
									NULL,
									&instance_got_ip));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_ESP_WIFI_SSID,
			.password = CONFIG_ESP_WIFI_PASSWORD,
			/* Setting a password implies station will connect to all security modes including WEP/WPA.
			 * However these modes are deprecated and not advisable to be used. Incase your Access point
			 * doesn't support WPA2, these mode can be enabled by commenting below line */
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,
			.pmf_cfg = {
				.capable = true,
				.required = false
			},
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start() );

	ESP_LOGI(TAG, "wifi_init_sta finished.");

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
		WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
		pdFALSE,
		pdFALSE,
		portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}
}
#endif // CONFIG_ST_MODE

void tcp_server(void *pvParameters);
void uart_event_task(void *pvParameters);
void udp_broadcast(void *pvParameters);
void spp_server(void *pvParameters);

void app_main()
{
	// Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	
#if CONFIG_AP_MODE
	// Initialize WiFi SoftAP
	ESP_LOGI(TAG, "ESP32 is WiFi AP MODE");
	wifi_init_softap();
	esp_netif_ip_info_t ip_info;
	ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info));
#endif

#if CONFIG_STA_MODE
	// Initialize WiFi STA
	ESP_LOGI(TAG, "ESP32 is WiFi STA MODE");
	wifi_init_sta();
	esp_netif_ip_info_t ip_info;
	ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info));
#endif

#if CONFIG_AP_MODE || CONFIG_STA_MODE
	/* Print the local IP address */
	ESP_LOGI(TAG, "IP Address : "  IPSTR, IP2STR(&ip_info.ip));
	ESP_LOGI(TAG, "Subnet mask: "  IPSTR, IP2STR(&ip_info.netmask));
	ESP_LOGI(TAG, "Gateway    : "  IPSTR, IP2STR(&ip_info.gw));
#endif

	/* Create Queue */
	xQueueCmd = xQueueCreate( 10, sizeof(CMD_t) );
	configASSERT( xQueueCmd );
	xQueueSend = xQueueCreate( 10, sizeof(CMD_t) );
	configASSERT( xQueueSend );

	// Create task
#if CONFIG_TCP_SOCKET
	xTaskCreate(tcp_server, "TCP", 1024*4, NULL, 5, NULL);
#endif
#if CONFIG_UDP_BROADCAST
	xTaskCreate(udp_broadcast, "UDP", 1024*4, NULL, 5, NULL);
#endif
#if CONFIG_BLUETOOTH_SPP
	xTaskCreate(spp_server, "SPP", 1024*4, NULL, 5, NULL);
#endif
	xTaskCreate(uart_event_task, "UART", 1024*4, NULL, 5, NULL);

	// Main loop
	bool connected = false;
	uint32_t sppHandle = 0;
	CMD_t cmdBuf;
	while(1) {
		xQueueReceive(xQueueCmd, &cmdBuf, portMAX_DELAY);
		ESP_LOGD(TAG, "cmdBuf.command=%d", cmdBuf.command);
		if (cmdBuf.command == CMD_CONNECT) {
			ESP_LOGI(TAG, "CMD_CONNECT");
			connected = true;
			sppHandle = cmdBuf.sppHandle;

		} else if (cmdBuf.command == CMD_DISCONNECT) {
			ESP_LOGI(TAG, "CMD_DISCONNECT");
			connected = false;
			sppHandle = 0;

		} else if (cmdBuf.command == CMD_NMEA) {
			if (connected == false) continue;
			cmdBuf.sppHandle = sppHandle;
			xQueueSend(xQueueSend, &cmdBuf, 0);
		}
	}

	// nerver reach here
	while (1) {
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}

}

