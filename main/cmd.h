#define CMD_NMEA        100
#define CMD_CONNECT     200
#define CMD_DISCONNECT  300

#define MAX_PAYLOAD     256
typedef struct {
    uint16_t command;
    size_t   length;
    uint8_t  payload[MAX_PAYLOAD];
	uint32_t sppHandle;
    TaskHandle_t taskHandle;
} CMD_t;

