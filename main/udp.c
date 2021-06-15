#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
//#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include "cmd.h"

#if CONFIG_UDP_BROADCAST

static const char *TAG = "UDP";

extern QueueHandle_t xQueueCmd;
extern QueueHandle_t xQueueSend;

// UDP Broadcast Task
void udp_broadcast(void *pvParameters)
{
	ESP_LOGI(TAG, "Start");
	//esp_log_level_set(TAG, ESP_LOG_WARN);

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
		xQueueReceive(xQueueSend, &cmdBuf, portMAX_DELAY);
		ESP_LOGD(TAG, "cmdBuf.command=%d", cmdBuf.command);
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

