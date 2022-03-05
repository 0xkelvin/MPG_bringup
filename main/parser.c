#include "parser.h"
#include "esp_log.h"
#include "define.h"
#include "connectWifi.h"
uint8_t wifi_ssid[32] = {};
uint8_t wifi_pwd[64];

uint8_t mqtt_topic[64];

uint8_t mqtt_user[64];
uint8_t mqtt_pwd[64];
char mqtt_server[64];
uint16_t port = 0;
uint8_t mode = 0;
uint8_t wifi_mode = 1;
uint8_t nbiot_mode = 1;
static const char *TAG = "Parser";
int getIndex(uint8_t value)
{

    int index = 0;
    if (value < 10)
    {
        return 1;
    }

    while (value >= 10)
    {
        value = value / 10;
        index = index + 1;
    }

    return index + 1;
}
int getIP(uint8_t *data)
{

    if (data)
    {
        memset(mqtt_server, 0, 64);

        int index = 0;
        ESP_LOGI("BLE PARSE", "%d", data[0]);

        sprintf(mqtt_server + index, "%d", data[0]);

        ESP_LOGI("BLE PARSE", "getIndex = %d", getIndex(data[0]));

        index += getIndex(data[0]);

        for (int i = 1; i < 4; i++)
        {
            ESP_LOGI("BLE PARSE", "%d", data[i]);
            ESP_LOGI("BLE PARSE", "getIndex = %d", getIndex(data[i]));

            sprintf(mqtt_server + index, ".%d", data[i]);
            index += getIndex(data[i]) + 1;
        }

        return 0;
    }
    else
    {
        return -1;
    }
}
int ble_parse(uint8_t *data, uint16_t len, ble_comm *ble_data)
{

    ESP_LOGI("BLE PARSE", "ble_parse");
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open
    ESP_LOGI(TAG, "Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t sensorNvs;
    // size_t required_size = 0;

    err = nvs_open(CONFIG_STORAGE, NVS_READWRITE, &sensorNvs);
    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }

    if (ble_data && data)
    {
        ble_data->start = data[0];
        ble_data->len = data[1];
        ble_data->cmd = data[2];
    }
    else
    {
        return -1;
    }

    switch (ble_data->cmd)
    {
    case SET_WIFI_HOSTNAME:
        memcpy(wifi_ssid, &data[DATA_OFSET], len - DATA_MAIN_LENGTH);
        wifi_ssid[len - DATA_MAIN_LENGTH] = '\0';
        set_wifi_ssid((char *)wifi_ssid);
        // ESP_LOGI(TAG, "SSID:%s-%d-%d\n", data, DATA_OFSET, len - DATA_MAIN_LENGTH);
        ESP_LOGI("BLE PARSE", "wifi ssid: %s\n", wifi_ssid);
        // err = nvs_set_str(sensorNvs, KEY_WIFI_HOST, (char *)wifi_ssid);
        // printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
        break;

    case SET_WIFI_PWD:
        memcpy(wifi_pwd, &data[DATA_OFSET], len - DATA_MAIN_LENGTH);
        wifi_pwd[len - DATA_MAIN_LENGTH] = '\0';
        ESP_LOGI("BLE PARSE", "wifi password: %s\n", wifi_pwd);
        set_wifi_password((char *)wifi_pwd);
        // err = nvs_set_str(sensorNvs, KEY_WIFI_PWD, (char *)wifi_pwd);
        // printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
        break;
    /*
    case SET_MQTT_SERVER:
        getIP(&data[DATA_OFSET]);
        ESP_LOGI("BLE PARSE", "mqtt server: %s", mqtt_server);
        err = nvs_set_str(sensorNvs, KEY_MQTT_SERVER, mqtt_server);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
        break;
    case SET_MQTT_TOPIC:
        memcpy(mqtt_topic, &data[DATA_OFSET], len - DATA_MAIN_LENGTH);
        mqtt_topic[len - DATA_MAIN_LENGTH] = '\0';
        ESP_LOGI("BLE PARSE", "mqtt topic: %s", mqtt_topic);
        err = nvs_set_str(sensorNvs, KEY_MQTT_TOPIC, (char *)mqtt_topic);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
        break;
    case SET_MQTT_PORT:
        port = (uint16_t)((data[DATA_OFSET] << 8) | data[DATA_OFSET + 1]);
        ESP_LOGI("BLE PARSE", "mqtt port: %hx", port);
        err = nvs_set_u16(sensorNvs, KEY_MQTT_PORT, port);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
        break;
    case SET_MQTT_USER:
        memcpy(mqtt_user, &data[DATA_OFSET], len - DATA_MAIN_LENGTH);
        mqtt_user[len - DATA_MAIN_LENGTH] = '\0';
        ESP_LOGI("BLE PARSE", "mqtt user: %s", mqtt_user);
        err = nvs_set_str(sensorNvs, KEY_MQTT_USER, (char *)mqtt_user);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
        break;
    case SET_MQTT_PWD:
        memcpy(mqtt_pwd, &data[DATA_OFSET], len - DATA_MAIN_LENGTH);
        mqtt_pwd[len - DATA_MAIN_LENGTH] = '\0';
        ESP_LOGI("BLE PARSE", "mqtt password: %s", mqtt_pwd);
        err = nvs_set_str(sensorNvs, KEY_MQTT_PWD, (char *)mqtt_pwd);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
        break;
    */
    case SET_MODE_POWER:
        mode = data[DATA_OFSET];
        ESP_LOGI("BLE PARSE", "mode: %d", mode);
        err = nvs_set_u8(sensorNvs, KEY_MODE_POWER, mode);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
        break;
    case ENABLED_WIFI:
        wifi_mode = data[DATA_OFSET];
        ESP_LOGI("BLE PARSE", "wifi_mode: %d", wifi_mode);
        err = nvs_set_u8(sensorNvs, KEY_WIFI_MODE, wifi_mode);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
        break;
    case ENABLED_NBIOT:
        nbiot_mode = data[DATA_OFSET];
        ESP_LOGI("BLE PARSE", "nbiot_mode: %d", nbiot_mode);
        err = nvs_set_u8(sensorNvs, KEY_NBIOT_MODE, nbiot_mode);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
        break;
    default:
        break;
    }
    err = nvs_commit(sensorNvs);
    printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
    fflush(stdout);
    nvs_close(sensorNvs);
    return 0;
}
