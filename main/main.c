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
#include "esp_spiffs.h"
#include "nvs_flash.h"

#include "driver/uart.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

static const char *TAG = "NMEA";

static const int RX_BUF_SIZE = 1024;

#define TXD_GPIO	0
// You have to set these CONFIG value using menuconfig.
#if 0
#define CONFIG_UART_RXD_GPIO	16
#endif

#define MAX_PAYLOAD		256
typedef struct {
	uint16_t command;
	size_t	 length;
	uint8_t  payload[MAX_PAYLOAD];
	TaskHandle_t taskHandle;
} CMD_t;

#define CMD_NMEA		100
#define CMD_CONNECT		200
#define CMD_DISCONNECT	250


typedef struct {
	char _ip[32];
	char _netmask[32];
	char _gw[32];
} NETWORK_t;

static QueueHandle_t xQueueCmd;
static QueueHandle_t xQueueTcp;
static QueueHandle_t uart0_queue;
static NETWORK_t myNetwork;

/* This project use WiFi configuration that you can set via 'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/

#define EXAMPLE_ESP_WIFI_SSID	   CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS	   CONFIG_ESP_WIFI_PASSWORD

#if CONFIG_AP_MODE
#define EXAMPLE_MAX_STA_CONN	   CONFIG_ESP_MAX_STA_CONN
#endif
#if CONFIG_ST_MODE
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY
#endif

#if CONFIG_ST_MODE
/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

/* The event group allows multiple bits for each event, but we only care about one event 
 * - are we connected to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;
#endif

static void event_handler(void* arg, esp_event_base_t event_base, 
								int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT) ESP_LOGI(TAG, "WIFI_EVENT event_id=%d", event_id);
	if (event_base == IP_EVENT) ESP_LOGI(TAG, "IP_EVENT event_id=%d", event_id);

#if CONFIG_AP_MODE
	if (event_id == WIFI_EVENT_AP_STACONNECTED) {
		wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
		ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
				 MAC2STR(event->mac), event->aid);
	} else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
		wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
		ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
				 MAC2STR(event->mac), event->aid);
	}
#endif

#if CONFIG_ST_MODE
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
			esp_wifi_connect();
			xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		}
		ESP_LOGI(TAG,"connect to the AP fail");
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
#if 0
		ESP_LOGI(TAG, "got ip:%s", ip4addr_ntoa(&event->ip_info.ip));
#endif
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

	//ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

	wifi_config_t wifi_config = {
		.ap = {
			.ssid = EXAMPLE_ESP_WIFI_SSID,
			.ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
			.password = EXAMPLE_ESP_WIFI_PASS,
			.max_connection = EXAMPLE_MAX_STA_CONN,
			.authmode = WIFI_AUTH_WPA_WPA2_PSK
		},
	};
	if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s",
			 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}
#endif

#if CONFIG_ST_MODE
bool parseAddress(int * ip, char * text) {
	ESP_LOGD(TAG, "parseAddress text=[%s]",text);
	int len = strlen(text);
	int octet = 0;
	char buf[4];
	int index = 0;
	for(int i=0;i<len;i++) {
		char c = text[i];
		if (c == '.') {
			ESP_LOGD(TAG, "buf=[%s] octet=%d", buf, octet);
			ip[octet] = strtol(buf, NULL, 10);
			octet++;
			index = 0;
		} else {
			if (index == 3) return false;
			if (c < '0' || c > '9') return false;
			buf[index++] = c;
			buf[index] = 0;
		}
	}

	if (strlen(buf) > 0) {
		ESP_LOGD(TAG, "buf=[%s] octet=%d", buf, octet);
		ip[octet] = strtol(buf, NULL, 10);
		octet++;
	}
	if (octet != 4) return false;
	return true;

}

void wifi_init_sta()
{
	s_wifi_event_group = xEventGroupCreate();
	ESP_ERROR_CHECK(esp_netif_init());

#if CONFIG_STATIC_IP

	ESP_LOGI(TAG, "CONFIG_STATIC_IP_ADDRESS=[%s]",CONFIG_STATIC_IP_ADDRESS);
	ESP_LOGI(TAG, "CONFIG_STATIC_GW_ADDRESS=[%s]",CONFIG_STATIC_GW_ADDRESS);
	ESP_LOGI(TAG, "CONFIG_STATIC_NM_ADDRESS=[%s]",CONFIG_STATIC_NM_ADDRESS);

	int ip[4];
	bool ret = parseAddress(ip, CONFIG_STATIC_IP_ADDRESS);
	ESP_LOGI(TAG, "parseAddress ret=%d ip=%d.%d.%d.%d", ret, ip[0], ip[1], ip[2], ip[3]);
	if (!ret) {
		ESP_LOGE(TAG, "CONFIG_STATIC_IP_ADDRESS [%s] not correct", CONFIG_STATIC_IP_ADDRESS);
	while(1) { vTaskDelay(1); }
	}

	int gw[4];
	ret = parseAddress(gw, CONFIG_STATIC_GW_ADDRESS);
	ESP_LOGI(TAG, "parseAddress ret=%d gw=%d.%d.%d.%d", ret, gw[0], gw[1], gw[2], gw[3]);
	if (!ret) {
		ESP_LOGE(TAG, "CONFIG_STATIC_GW_ADDRESS [%s] not correct", CONFIG_STATIC_GW_ADDRESS);
	while(1) { vTaskDelay(1); }
	}

	int nm[4];
	ret = parseAddress(nm, CONFIG_STATIC_NM_ADDRESS);
	ESP_LOGI(TAG, "parseAddress ret=%d nm=%d.%d.%d.%d", ret, nm[0], nm[1], nm[2], nm[3]);
	if (!ret) {
		ESP_LOGE(TAG, "CONFIG_STATIC_NM_ADDRESS [%s] not correct", CONFIG_STATIC_NM_ADDRESS);
	while(1) { vTaskDelay(1); }
	}

	tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);

	/* Set STATIC IP Address */
	tcpip_adapter_ip_info_t ipInfo;
	//IP4_ADDR(&ipInfo.ip, 192,168,10,100);
	IP4_ADDR(&ipInfo.ip, ip[0], ip[1], ip[2], ip[3]);
	IP4_ADDR(&ipInfo.gw, gw[0], gw[1], gw[2], gw[3]);
	IP4_ADDR(&ipInfo.netmask, nm[0], nm[1], nm[2], nm[3]);
	tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);

#endif

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = EXAMPLE_ESP_WIFI_SSID,
			.password = EXAMPLE_ESP_WIFI_PASS
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start() );

	ESP_LOGI(TAG, "wifi_init_sta finished.");
	ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
			 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);

	// wait for IP_EVENT_STA_GOT_IP
	while(1) {
		/* Wait forever for WIFI_CONNECTED_BIT to be set within the event group.
		   Clear the bits beforeexiting. */
		EventBits_t uxBits = xEventGroupWaitBits(s_wifi_event_group,
		   WIFI_CONNECTED_BIT, /* The bits within the event group to waitfor. */
		   pdTRUE,		  /* WIFI_CONNECTED_BIT should be cleared before returning. */
		   pdFALSE,		  /* Don't waitfor both bits, either bit will do. */
		   portMAX_DELAY);/* Wait forever. */
	   if ( ( uxBits & WIFI_CONNECTED_BIT ) == WIFI_CONNECTED_BIT ){
		   ESP_LOGI(TAG, "WIFI_CONNECTED_BIT");
		   break;
	   }
	}
	ESP_LOGI(TAG, "Got IP Address.");
}
#endif


static void uart_event_task(void *pvParameters)
{
	ESP_LOGI(pcTaskGetTaskName(0), "Start");
	esp_log_level_set(pcTaskGetTaskName(0), ESP_LOG_WARN);

	uart_event_t event;
	size_t buffered_size;
	uint8_t* rxdata = (uint8_t*) malloc(RX_BUF_SIZE);
	CMD_t cmdBuf;
	cmdBuf.command = CMD_NMEA;
	cmdBuf.taskHandle = xTaskGetCurrentTaskHandle();

	for(;;) {
		//Waiting for UART event.
		if(xQueueReceive(uart0_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
			bzero(rxdata, RX_BUF_SIZE);
			ESP_LOGI(pcTaskGetTaskName(0), "uart[%d] event:", UART_NUM_1);
			switch(event.type) {
				//Event of UART receving data
				/*We'd better handler data event fast, there would be much more data events than
				other types of events. If we take too much time on data event, the queue might
				be full.*/
				case UART_DATA:
					ESP_LOGI(pcTaskGetTaskName(0), "[UART DATA]: %d", event.size);
					break;
				//Event of HW FIFO overflow detected
				case UART_FIFO_OVF:
					ESP_LOGW(pcTaskGetTaskName(0), "hw fifo overflow");
					// If fifo overflow happened, you should consider adding flow control for your application.
					// The ISR has already reset the rx FIFO,
					// As an example, we directly flush the rx buffer here in order to read more data.
					uart_flush_input(UART_NUM_1);
					xQueueReset(uart0_queue);
					break;
				//Event of UART ring buffer full
				case UART_BUFFER_FULL:
					ESP_LOGW(pcTaskGetTaskName(0), "ring buffer full");
					// If buffer full happened, you should consider encreasing your buffer size
					// As an example, we directly flush the rx buffer here in order to read more data.
					uart_flush_input(UART_NUM_1);
					xQueueReset(uart0_queue);
					break;
				//Event of UART RX break detected
				case UART_BREAK:
					ESP_LOGW(pcTaskGetTaskName(0), "uart rx break");
					break;
				//Event of UART parity check error
				case UART_PARITY_ERR:
					ESP_LOGW(pcTaskGetTaskName(0), "uart parity error");
					break;
				//Event of UART frame error
				case UART_FRAME_ERR:
					ESP_LOGW(pcTaskGetTaskName(0), "uart frame error");
					break;
				//UART_PATTERN_DET
				case UART_PATTERN_DET:
					uart_get_buffered_data_len(UART_NUM_1, &buffered_size);
					int pos = uart_pattern_pop_pos(UART_NUM_1);
					ESP_LOGI(pcTaskGetTaskName(0), "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
					if (pos == -1) {
						// There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
						// record the position. We should set a larger queue size.
						// As an example, we directly flush the rx buffer here.
						uart_flush_input(UART_NUM_1);
					} else {
						uart_read_bytes(UART_NUM_1, rxdata, buffered_size, 100 / portTICK_PERIOD_MS);
						ESP_LOGI(pcTaskGetTaskName(0), "rxdata[%s]", rxdata);
#if 0
						cmdBuf.length = buffered_size - 2;
						memcpy((char *)cmdBuf.payload, (char *)rxdata, buffered_size - 2); 
						cmdBuf.payload[buffered_size-2] = 0;
#endif
						// Without crlf, u-center does not recognize NMEA
						cmdBuf.length = buffered_size;
						memcpy((char *)cmdBuf.payload, (char *)rxdata, buffered_size); 
						cmdBuf.payload[buffered_size] = 0;
						ESP_LOGI(pcTaskGetTaskName(0), "cmdBuf.payload[%s]", cmdBuf.payload);
						xQueueSend(xQueueCmd, &cmdBuf, 0);
					}
					break;
				//Others
				default:
					ESP_LOGW(pcTaskGetTaskName(0), "uart event type: %d", event.type);
					break;
			}
		}
	}
	// never reach
	free(rxdata);
	rxdata = NULL;
	vTaskDelete(NULL);
}


#if CONFIG_TCP_SOCKET
// TCP Server Task
void tcp_server(void *pvParameters)
{
	ESP_LOGI(pcTaskGetTaskName(0), "Start");
	//esp_log_level_set(pcTaskGetTaskName(0), ESP_LOG_WARN);

	/* set up address to connect to */
	struct sockaddr_in srcAddr;
	struct sockaddr_in dstAddr;
	memset(&srcAddr, 0, sizeof(srcAddr));
	//srcAddr.sin_len = sizeof(srcAddr);
	srcAddr.sin_family = AF_INET;
	//srcAddr.sin_port = PP_HTONS(SERVER_PORT);
	srcAddr.sin_port = htons(CONFIG_SERVER_PORT);
	srcAddr.sin_addr.s_addr = INADDR_ANY;

	/* create the socket */
	int srcSocket;
	int dstSocket;
	socklen_t dstAddrSize;
	int ret;

	srcSocket = lwip_socket(AF_INET, SOCK_STREAM, 0);
	LWIP_ASSERT("srcSocket >= 0", srcSocket >= 0);

	/* bind socket */
	ret = lwip_bind(srcSocket, (struct sockaddr *)&srcAddr, sizeof(srcAddr));
	/* should succeed */
	LWIP_ASSERT("ret == 0", ret == 0);

	/* listen socket */
	ret = lwip_listen(srcSocket, 5);
	/* should succeed */
	LWIP_ASSERT("ret == 0", ret == 0);

	CMD_t cmdBuf;
	cmdBuf.taskHandle = xTaskGetCurrentTaskHandle();
	cmdBuf.length = 0;
	int	connected;

	while(1) {
		// Connection acceptance
		ESP_LOGI(pcTaskGetTaskName(0), "Wait from client connect port:%d", CONFIG_SERVER_PORT);
		dstAddrSize = sizeof(dstAddr);
		dstSocket = lwip_accept(srcSocket, (struct sockaddr *)&dstAddr, &dstAddrSize);
		ESP_LOGI(pcTaskGetTaskName(0), "Connect from %s",inet_ntoa(dstAddr.sin_addr));
		cmdBuf.command = CMD_CONNECT;
		xQueueSend(xQueueCmd, &cmdBuf, 0); // Send CONNECT
		connected = 1;

		while(connected) {
			xQueueReceive(xQueueTcp, &cmdBuf, portMAX_DELAY);
			ESP_LOGD(pcTaskGetTaskName(0), "cmdBuf.command=%d", cmdBuf.command);
			if (cmdBuf.command != CMD_NMEA) continue;
			ESP_LOGD(pcTaskGetTaskName(0), "[%s] payload=[%.*s]",__FUNCTION__, cmdBuf.length, cmdBuf.payload);
			/* write something */
			ret = lwip_write(dstSocket, cmdBuf.payload, cmdBuf.length);
#if 0
				uint8_t buffer[64];
				sprintf((char *)buffer,"$GPGSA,A,1,,,,,,,,,,,,,99.99,99.99,99.99*30\r\n");
				ret = lwip_write(dstSocket, buffer, strlen((char *)buffer));
#endif
			ESP_LOGI(pcTaskGetTaskName(0), "lwip_write ret=%d", ret);
			if (ret < 0) {
				cmdBuf.command = CMD_DISCONNECT;
				xQueueSend(xQueueCmd, &cmdBuf, 0); // Send DISCONNECT
				connected = 0;
			}
		} // end while
	} // end for


	/* close socket. Don't reach here.*/
	ret = lwip_close(srcSocket);
	LWIP_ASSERT("ret == 0", ret == 0);
	vTaskDelete( NULL );

}
#endif

#if CONFIG_UDP_BROADCAST
// UDP Broadcast Task
void udp_broadcast(void *pvParameters)
{
    ESP_LOGI(pcTaskGetTaskName(0), "Start");
    //esp_log_level_set(pcTaskGetTaskName(0), ESP_LOG_WARN);

	/* set up address to sendto */
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(CONFIG_BROADCAST_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_BROADCAST); /* send message to 255.255.255.255 */

	/* create the socket */
	int fd;
	int ret;
	fd = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP ); // Create a UDP socket.
	LWIP_ASSERT("fd >= 0", fd >= 0);

	CMD_t cmdBuf;
	cmdBuf.taskHandle = xTaskGetCurrentTaskHandle();
	cmdBuf.length = 0;
	cmdBuf.command = CMD_CONNECT;
	xQueueSend(xQueueCmd, &cmdBuf, 0); // Send CONNECT

	while(1) {
		xQueueReceive(xQueueTcp, &cmdBuf, portMAX_DELAY);
		ESP_LOGD(pcTaskGetTaskName(0), "cmdBuf.command=%d", cmdBuf.command);
		if (cmdBuf.command != CMD_NMEA) continue;
		ret = lwip_sendto(fd, cmdBuf.payload, cmdBuf.length, 0, (struct sockaddr *)&addr, sizeof(addr));
		LWIP_ASSERT("ret == cmdBuf.length", ret == cmdBuf.length);
	}

	/* close socket. Don't reach here.*/
	ret = lwip_close(fd);
	LWIP_ASSERT("ret == 0", ret == 0);
	vTaskDelete( NULL );
}
#endif


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
	ESP_LOGI(TAG, "WIFI_MODE_AP");
	wifi_init_softap();
	tcpip_adapter_ip_info_t ip_info;
	ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info));
	ESP_LOGI(TAG, "ESP32 is AP MODE");
#endif

#if CONFIG_ST_MODE
	// Initialize WiFi STA
	ESP_LOGI(TAG, "WIFI_MODE_STA");
	wifi_init_sta();
	tcpip_adapter_ip_info_t ip_info;
	ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
	ESP_LOGI(TAG, "ESP32 is STA MODE");
#endif

	/* Print the local IP address */
	ESP_LOGI(TAG, "IP Address:	%s", ip4addr_ntoa(&ip_info.ip));
	ESP_LOGI(TAG, "Subnet mask: %s", ip4addr_ntoa(&ip_info.netmask));
	ESP_LOGI(TAG, "Gateway:		%s", ip4addr_ntoa(&ip_info.gw));
	sprintf(myNetwork._ip, "%s", ip4addr_ntoa(&ip_info.ip));
	sprintf(myNetwork._netmask, "%s", ip4addr_ntoa(&ip_info.netmask));
	sprintf(myNetwork._gw, "%s", ip4addr_ntoa(&ip_info.gw));

	ESP_LOGI(TAG, "Initializing UART");
	/* Configure parameters of an UART driver,
	 * communication pins and install the driver */
	uart_config_t uart_config = {
		.baud_rate = 9600,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};
	uart_param_config(UART_NUM_1, &uart_config);

	//Set UART pins (using UART0 default pins ie no changes.)
	uart_set_pin(UART_NUM_1, TXD_GPIO, CONFIG_UART_RXD_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	//Install UART driver, and get the queue.
	uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 20, &uart0_queue, 0);

	//Set uart pattern detect function.
	//uart_enable_pattern_det_intr(UART_NUM_1, 0x0a, 1, 10000, 10, 10); // pattern is LF
	uart_enable_pattern_det_baud_intr(UART_NUM_1, 0x0a, 1, 9, 0, 0); // pattern is LF
	//Reset the pattern queue length to record at most 20 pattern positions.
	uart_pattern_queue_reset(UART_NUM_1, 20);
	ESP_LOGI(TAG, "Initializing UART done");


	/* Create Queue */
	xQueueCmd = xQueueCreate( 10, sizeof(CMD_t) );
	configASSERT( xQueueCmd );
	xQueueTcp = xQueueCreate( 10, sizeof(CMD_t) );
	configASSERT( xQueueTcp );

#if CONFIG_TCP_SOCKET
	xTaskCreate(tcp_server, "TCP SERVER", 1024*4, NULL, 5, NULL);
#endif
#if CONFIG_UDP_BROADCAST
	xTaskCreate(udp_broadcast, "UDP BROADCAST", 1024*4, NULL, 5, NULL);
#endif
	//Create a task to handler UART event from ISR
	xTaskCreate(uart_event_task, "uart_event", 1024*4, NULL, 5, NULL);

	// Main loop
	bool connected = false;
	CMD_t cmdBuf;
	while(1) {
		xQueueReceive(xQueueCmd, &cmdBuf, portMAX_DELAY);
		ESP_LOGD(TAG, "cmdBuf.command=%d", cmdBuf.command);
		if (cmdBuf.command == CMD_CONNECT) {
			ESP_LOGI(TAG, "CMD_CONNECT");
			connected = true;

		} else if (cmdBuf.command == CMD_DISCONNECT) {
			ESP_LOGI(TAG, "CMD_DISCONNECT");
			connected = false;

		} else if (cmdBuf.command == CMD_NMEA) {
			if (connected) xQueueSend(xQueueTcp, &cmdBuf, 0);
		}
	}

	// nerver reach
	while (1) {
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}

}

