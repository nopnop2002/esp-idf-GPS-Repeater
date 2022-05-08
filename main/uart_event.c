/* UART receiver task

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
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"

#include "driver/uart.h"

#include "cmd.h"

static const int RX_BUF_SIZE = 1024;

#define TXD_GPIO	0
// You have to set these CONFIG value using menuconfig.
//#define CONFIG_UART_RXD_GPIO	16

extern QueueHandle_t xQueueCmd;

void uart_event_task(void *pvParameters)
{
	ESP_LOGI(pcTaskGetName(0), "Start");
	esp_log_level_set(pcTaskGetName(0), ESP_LOG_WARN);

	ESP_LOGI(pcTaskGetName(0), "Initializing UART");
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
	ESP_LOGI(pcTaskGetName(0), "CONFIG_UART_RXD_GPIO=%d", CONFIG_UART_RXD_GPIO);
	uart_set_pin(UART_NUM_1, TXD_GPIO, CONFIG_UART_RXD_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

	//Install UART driver, and get the queue handle.
	QueueHandle_t uart0_queue;
	uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 20, &uart0_queue, 0);

	//Set uart pattern detect function.
	//uart_enable_pattern_det_intr(UART_NUM_1, 0x0a, 1, 10000, 10, 10); // pattern is LF
	uart_enable_pattern_det_baud_intr(UART_NUM_1, 0x0a, 1, 9, 0, 0); // pattern is LF
	//Reset the pattern queue length to record at most 20 pattern positions.
	uart_pattern_queue_reset(UART_NUM_1, 20);
	ESP_LOGI(pcTaskGetName(0), "Initializing UART done");

	uart_event_t event;
	size_t buffered_size;
	uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE);
	CMD_t cmdBuf;
	cmdBuf.command = CMD_NMEA;
	cmdBuf.taskHandle = xTaskGetCurrentTaskHandle();

	while(1) {
		//Waiting for UART event.
		if(xQueueReceive(uart0_queue, (void * )&event, portMAX_DELAY)) {
			bzero(data, RX_BUF_SIZE);
			ESP_LOGI(pcTaskGetName(0), "uart[%d] event:", UART_NUM_1);
			switch(event.type) {
				//Event of UART receving data
				/*We'd better handler data event fast, there would be much more data events than
				other types of events. If we take too much time on data event, the queue might
				be full.*/
				case UART_DATA:
					ESP_LOGI(pcTaskGetName(0), "[UART DATA]: %d", event.size);
					break;
				//Event of HW FIFO overflow detected
				case UART_FIFO_OVF:
					ESP_LOGW(pcTaskGetName(0), "hw fifo overflow");
					// If fifo overflow happened, you should consider adding flow control for your application.
					// The ISR has already reset the rx FIFO,
					// As an example, we directly flush the rx buffer here in order to read more data.
					uart_flush_input(UART_NUM_1);
					xQueueReset(uart0_queue);
					break;
				//Event of UART ring buffer full
				case UART_BUFFER_FULL:
					ESP_LOGW(pcTaskGetName(0), "ring buffer full");
					// If buffer full happened, you should consider encreasing your buffer size
					// As an example, we directly flush the rx buffer here in order to read more data.
					uart_flush_input(UART_NUM_1);
					xQueueReset(uart0_queue);
					break;
				//Event of UART RX break detected
				case UART_BREAK:
					ESP_LOGW(pcTaskGetName(0), "uart rx break");
					break;
				//Event of UART parity check error
				case UART_PARITY_ERR:
					ESP_LOGW(pcTaskGetName(0), "uart parity error");
					break;
				//Event of UART frame error
				case UART_FRAME_ERR:
					ESP_LOGW(pcTaskGetName(0), "uart frame error");
					break;
				//UART_PATTERN_DET
				case UART_PATTERN_DET:
					uart_get_buffered_data_len(UART_NUM_1, &buffered_size);
					int pos = uart_pattern_pop_pos(UART_NUM_1);
					ESP_LOGI(pcTaskGetName(0), "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
					if (pos == -1) {
						// There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
						// record the position. We should set a larger queue size.
						// As an example, we directly flush the rx buffer here.
						uart_flush_input(UART_NUM_1);
					} else {
						uart_read_bytes(UART_NUM_1, data, buffered_size, 100 / portTICK_PERIOD_MS);
						ESP_LOGI(pcTaskGetName(0), "read data: %s", data);
						ESP_LOG_BUFFER_HEXDUMP(pcTaskGetName(0), data, buffered_size, ESP_LOG_INFO);
#if 0
I (53691) uart_event: [UART PATTERN DETECTED] pos: 68, buffered size: 69
I (53701) uart_event: 0x3ffceb48	 24 47 50 52 4d 43 2c 31	32 33 39 31 32 2c 41 2c  |$GPRMC,123912,A,|
I (53711) uart_event: 0x3ffceb58	 33 35 34 31 2e 30 31 36	32 38 33 2c 4e 2c 31 33  |3541.016283,N,13|
I (53721) uart_event: 0x3ffceb68	 39 34 36 2e 32 35 38 31	35 30 2c 45 2c 30 2e 30  |946.258150,E,0.0|
I (53731) uart_event: 0x3ffceb78	 2c 32 35 2e 33 2c 30 37	30 35 32 32 2c 2c 2c 41  |,25.3,070522,,,A|
I (53741) uart_event: 0x3ffceb88	 2a 34 33 0d 0a																		 |*43..|
#endif
						cmdBuf.length = buffered_size;
						memcpy((char *)cmdBuf.payload, (char *)data, buffered_size); 
						cmdBuf.payload[buffered_size] = 0;
						xQueueSend(xQueueCmd, &cmdBuf, 0);
					}
					break;
				//Others
				default:
					ESP_LOGW(pcTaskGetName(0), "uart event type: %d", event.type);
					break;
			} // end switch
		} // end if
	} // end while

	// never reach here
	free(data);
	data = NULL;
	vTaskDelete(NULL);
}

