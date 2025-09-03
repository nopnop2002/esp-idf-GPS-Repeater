/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"

#include "cmd.h"

#define SPP_TAG "SPP_ACCEPTOR"
#define SPP_SERVER_NAME "SPP_SERVER"
#define DEVICE_NAME "ESP_SPP_ACCEPTOR"

static const esp_spp_mode_t esp_spp_mode = ESP_SPP_MODE_CB;

static const esp_spp_sec_t sec_mask = ESP_SPP_SEC_AUTHENTICATE;
static const esp_spp_role_t role_slave = ESP_SPP_ROLE_SLAVE;

extern QueueHandle_t xQueueCmd;
extern QueueHandle_t xQueueSend;

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
	CMD_t cmdBuf;
	switch (event) {
	case ESP_SPP_INIT_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_INIT_EVT");
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
		esp_bt_gap_set_device_name(DEVICE_NAME);
#else
		esp_bt_dev_set_device_name(DEVICE_NAME);
#endif
		esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
		esp_spp_start_srv(sec_mask,role_slave, 0, SPP_SERVER_NAME);
		break;
	case ESP_SPP_DISCOVERY_COMP_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_DISCOVERY_COMP_EVT");
		break;
	case ESP_SPP_OPEN_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_OPEN_EVT");
		break;
	case ESP_SPP_CLOSE_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_CLOSE_EVT");
		cmdBuf.command = CMD_DISCONNECT;
		cmdBuf.sppHandle = 0;
		xQueueSend(xQueueCmd, &cmdBuf, 0);
		break;
	case ESP_SPP_START_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_START_EVT");
		break;
	case ESP_SPP_CL_INIT_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_CL_INIT_EVT");
		break;
	case ESP_SPP_DATA_IND_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_DATA_IND_EVT len=%d handle=%"PRIu32,
				 param->data_ind.len, param->data_ind.handle);
		ESP_LOG_BUFFER_HEXDUMP(SPP_TAG, param->data_ind.data, param->data_ind.len, ESP_LOG_INFO);
		break;
	case ESP_SPP_CONG_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_CONG_EVT");
		break;
	case ESP_SPP_WRITE_EVT:
		ESP_LOGD(SPP_TAG, "ESP_SPP_WRITE_EVT");
		break;
	case ESP_SPP_SRV_OPEN_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_OPEN_EVT");
		cmdBuf.command = CMD_CONNECT;
		cmdBuf.sppHandle = param->srv_open.handle;
		xQueueSend(xQueueCmd, &cmdBuf, 0);
		break;
	case ESP_SPP_SRV_STOP_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_STOP_EVT");
		break;
	case ESP_SPP_UNINIT_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_UNINIT_EVT");
		break;
	default:
		break;
	}
}

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
	switch (event) {
		case ESP_BT_GAP_AUTH_CMPL_EVT:{
			if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
				ESP_LOGI(SPP_TAG, "authentication success: %s", param->auth_cmpl.device_name);
				ESP_LOG_BUFFER_HEXDUMP(SPP_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN, ESP_LOG_INFO);
			} else {
				ESP_LOGE(SPP_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
			}
			break;
		}
		case ESP_BT_GAP_PIN_REQ_EVT:{
			ESP_LOGI(SPP_TAG, "ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d", param->pin_req.min_16_digit);
			if (param->pin_req.min_16_digit) {
				ESP_LOGI(SPP_TAG, "Input pin code: 0000 0000 0000 0000");
				esp_bt_pin_code_t pin_code = {0};
				esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
			} else {
				ESP_LOGI(SPP_TAG, "Input pin code: 1234");
				esp_bt_pin_code_t pin_code;
				pin_code[0] = '1';
				pin_code[1] = '2';
				pin_code[2] = '3';
				pin_code[3] = '4';
				esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
			}
			break;
		}

		case ESP_BT_GAP_MODE_CHG_EVT:
			ESP_LOGI(SPP_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode:%d", param->mode_chg.mode);
			break;

		default: {
			ESP_LOGI(SPP_TAG, "event: %d", event);
			break;
		}
	}
	return;
}


void spp_server(void *pvParameters)
{
	ESP_LOGI(SPP_TAG, "Start");
	esp_err_t ret;

	ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}

	if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}

	if ((ret = esp_bluedroid_init()) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s initialize bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}

	if ((ret = esp_bluedroid_enable()) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}

	if ((ret = esp_bt_gap_register_callback(esp_bt_gap_cb)) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s gap register failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}

	if ((ret = esp_spp_register_callback(esp_spp_cb)) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s spp register failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
	esp_spp_cfg_t bt_spp_cfg = {
		.mode = esp_spp_mode,
		.enable_l2cap_ertm = true,
		.tx_buffer_size = 0, /* Only used for ESP_SPP_MODE_VFS mode */
	};
	if ((ret = esp_spp_enhanced_init(&bt_spp_cfg)) != ESP_OK) {
#else
	if ((ret = esp_spp_init(esp_spp_mode)) != ESP_OK) {
#endif
		ESP_LOGE(SPP_TAG, "%s spp init failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}

	/*
	 * Set default parameters for Legacy Pairing
	 * Use variable pin, input pin code when pairing
	 */
	esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
	esp_bt_pin_code_t pin_code;
	esp_bt_gap_set_pin(pin_type, 0, pin_code);

	CMD_t cmdBuf;
	while(1) {
		xQueueReceive(xQueueSend, &cmdBuf, portMAX_DELAY);
		ESP_LOGD(SPP_TAG,"cmdBuf.command=%d sppHandle=%"PRIu32, cmdBuf.command, cmdBuf.sppHandle);
		if (cmdBuf.command == CMD_NMEA) {
			if (cmdBuf.sppHandle == 0) continue;
			esp_spp_write(cmdBuf.sppHandle, cmdBuf.length, cmdBuf.payload);
		}
	}
}
