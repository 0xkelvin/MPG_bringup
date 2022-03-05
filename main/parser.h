#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

typedef struct
{
    uint8_t start;
    uint8_t len;
    uint8_t cmd;
    uint8_t *data;
} ble_comm;

typedef enum
{
    DONE = 0X00,
    GET_DEVICE = 0X01,
    SET_MQTT_SERVER = 0X02,
    SET_MQTT_TOPIC = 0X03,
    SET_MQTT_USER = 0X04,
    SET_MQTT_PWD = 0X05,
    SET_MQTT_PORT = 0X06,
    SET_MODE_POWER = 0X09,
    SET_WIFI_HOSTNAME = 0X10,
    SET_WIFI_PWD = 0X11,
    CONNECT_WIFI = 0X12,
    SCAN_WIFI = 0X13,
    ENABLED_WIFI = 0X14,
    ENABLED_NBIOT = 0X15,
    GET_STATUS_NETWORK = 0X16
} Command;

extern uint8_t wifi_ssid[32]; /**< SSID of target AP. */
extern uint8_t wifi_pwd[64];  /**< Password of target AP. */

extern uint8_t mqtt_topic[64];
extern uint8_t mqtt_user[64];
extern uint8_t mqtt_pwd[64];
extern char mqtt_server[64];
extern uint8_t mqtt_port[2];

int ble_parse(uint8_t *data, uint16_t len, ble_comm *ble_data);
