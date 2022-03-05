#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "connectWifi.h"
#include "parser.h"
#include "define.h"
#include "cJSON.h"
#include "common.h"
#include "gatt_server.h"
/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
//#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
//#define EXAMPLE_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
// uint8_t wifi_ssid_[BUF_SIZE_32] = {};
// uint8_t wifi_pwd_[BUF_SIZE_64] = {};
#define EXAMPLE_ESP_MAXIMUM_RETRY 2

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
static const char *TAG = "wifi station";
uint8_t _listSSID[BUF_SIZE_128 * 2] = {0};
static int s_retry_num = 0;
#define DEFAULT_WIFI_SSID "mywifissid"
#define DEFAULT_WIFI_PASS "mywifipass"
static wifi_config_t wifi_config = {
    .sta = {
        .ssid = DEFAULT_WIFI_SSID,
        .password = DEFAULT_WIFI_PASS,
        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        .pmf_cfg = {
            .capable = true,
            .required = false},
    },
};
static char *_wifiInfo = NULL;
static uint16_t ap_count = 0;
static uint8_t ap_index = 0;
wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
// if connected not read NSV other Ap
static bool wifi_connect_ok = false;
char *wifiInfo(void)
{
    return _wifiInfo;
}
static void connect_ap_index()
{
    int i = 0;
    // for (i = ap_index; i < ap_count; i++)
    for (i = ap_index; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++)
    {
        wifi_config.sta.threshold.authmode = ap_info[i].authmode;
        if (ap_info[i].authmode == WIFI_AUTH_OPEN)
        {
            memcpy(wifi_config.sta.ssid, ap_info[i].ssid, BUF_SIZE_32);
            memset(wifi_config.sta.password, 0, BUF_SIZE_64);
            ap_index = i;
            ESP_LOGI(TAG, "AP[%d] = %s 'No pass'", i, ap_info[i].ssid);
            break;
        }
        else
        {
            char *pass_wifi = read_pass_wifi_by_ssid((char *)ap_info[i].ssid);
            if (pass_wifi != NULL)
            {
                memcpy(wifi_config.sta.ssid, ap_info[i].ssid, BUF_SIZE_32);
                memcpy(wifi_config.sta.password, pass_wifi, BUF_SIZE_64);
                ap_index = i;
                break;
            }
        }
    }
    if (i < ap_count && i < DEFAULT_SCAN_LIST_SIZE)
    {
        ESP_LOGI(TAG, "Next Station Wifi[%d]:%s PASS:%s", i, wifi_config.sta.ssid, wifi_config.sta.password);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        esp_wifi_connect();
        s_retry_num = 0;
    }
    else
    {
        ESP_LOGI(TAG, "AP not found in NSV");
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
}
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "Wifi start connect");
        wifi_connect_ok = false;
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        if (WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT == event->reason || WIFI_REASON_AUTH_FAIL == event->reason)
        {
            ESP_LOGE(TAG, " Wifi disconect reason incorrect WiFi Credentials: %d", event->reason);
        }
        else
            ESP_LOGE(TAG, "Wifi disconect reason: %d", event->reason);
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to SSID:%s PASS:%s", wifi_config.sta.ssid, wifi_config.sta.password);
        }
        else
        {
            if (ap_index == ap_count)
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            else
            {
                // Next AP
                ap_index++;
                connect_ap_index();
            }
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        ESP_LOGI(TAG, "Connected to the wifi AP:%s, Waiting got IP address", wifi_config.sta.ssid);
        // wifi_connect_ok = true;
        // xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_STOP)
    {
        ESP_LOGI(TAG, "Wifi  Even Stop.");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE)
    {
        uint16_t number = DEFAULT_SCAN_LIST_SIZE;
        // wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
        memset(ap_info, 0, sizeof(ap_info));
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
        char iTemssid[40] = {0};
        sprintf(iTemssid, "%s,%d", ap_info[0].ssid, ap_info[0].rssi);
        memcpy(_listSSID, iTemssid, sizeof(iTemssid));
        for (int i = 1; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++)
        {
            sprintf(iTemssid, "%s,%d", ap_info[i].ssid, ap_info[i].rssi);
            strcat((char *)_listSSID, "|");
            strcat((char *)_listSSID, iTemssid);
            // ESP_LOGE(TAG, "%s: mode %d", ap_info[i].ssid, ap_info[i].authmode);
        }
        // xTaskCreate(&light_red_blu_mix, "light_red_blu_mix", 1024, (void *)5000, 5, NULL);
        ESP_LOGI(TAG, "Total APs scanned: %u:%s", ap_count, _listSSID);
        if (wifi_connect_ok == false)
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "ip:" IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "gw:" IPSTR, IP2STR(&event->ip_info.gw));
        ESP_LOGI(TAG, "mask:" IPSTR, IP2STR(&event->ip_info.netmask));
        s_retry_num = 0;
        wifi_ap_record_t apinfo;
        esp_wifi_sta_get_ap_info(&apinfo);
        wifi_connect_ok = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        cJSON *_JsonTem = cJSON_CreateObject();

        // cJSON_AddStringToObject(_JsonTem, "net", "W");
        cJSON_AddStringToObject(_JsonTem, "ssid", (char *)apinfo.ssid);
        // ip
        char ipInfo_str[BUF_SIZE_32] = {0};
        sprintf(ipInfo_str, "" IPSTR, IP2STR(&event->ip_info.ip));
        cJSON_AddStringToObject(_JsonTem, "ip", ipInfo_str);
        // mac bssid
        char mac_str[18];
        sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", apinfo.bssid[0], apinfo.bssid[1], apinfo.bssid[2], apinfo.bssid[3], apinfo.bssid[4], apinfo.bssid[5]);
        cJSON_AddStringToObject(_JsonTem, "mac", mac_str);
        // rssi
        cJSON_AddNumberToObject(_JsonTem, "rssi", apinfo.rssi);
        _wifiInfo = cJSON_PrintUnformatted(_JsonTem);
        cJSON_Delete(_JsonTem);
        ESP_LOGI(TAG, "Wifi Info:%s", _wifiInfo);
    }
    else
        ESP_LOGI(TAG, "Other Even:%d", event_id);
}
esp_err_t wifi_init_sta(char *sta_hostname)
{
    ESP_LOGI(TAG, "WIFI TESTING");

    if (s_wifi_event_group != NULL)
        vEventGroupDelete(s_wifi_event_group);
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, &instance_got_ip));
    tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, sta_hostname);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_cfg;
    if (esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_cfg) != ESP_OK)
    {
        // set default AP
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_LOGI(TAG, "Connect with AP default SSID: %s, PASS:%s", wifi_config.sta.ssid, wifi_config.sta.password);
        // return ESP_FAIL;
    }
    else
    {
        if (strlen((const char *)wifi_cfg.sta.ssid) == 0)
        {
            ESP_LOGI(TAG, "Set default Wifi.");
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        }
        ESP_LOGI(TAG, "Connect with AP SSID:%s, PASS:%s", wifi_cfg.sta.ssid, wifi_cfg.sta.password);
        wifi_config = wifi_cfg;
    }
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_err_t err = ESP_FAIL;
    // const TickType_t xTicksToWait = 1000 / portTICK_PERIOD_MS;
    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           10000); // portMAX_DELAY
    wifiScan();
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Connected with AP default");
        err = ESP_OK;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "If the connection fails then scan saved wifi reconnect to saved wifi");
        // If the connection fails then scan saved wifi reconnect to saved wifi
        // Scan done event will be called WIFI_EVENT_SCAN_DONE and reassign ssid and password different, reconnect
        vEventGroupDelete(s_wifi_event_group);
        if (ap_index < ap_count)
        {
            s_wifi_event_group = xEventGroupCreate();
            connect_ap_index();
            bits = xEventGroupWaitBits(s_wifi_event_group,
                                       WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                       pdFALSE,
                                       pdFALSE,
                                       10000); // portMAX_DELAY
            if (bits & WIFI_CONNECTED_BIT)
            {
                // If the connection is successful, save the ssid and password into NSV
                ESP_LOGI(TAG, "Connected to ap Wifi AP:%s password:%s",
                         wifi_config.sta.ssid, wifi_config.sta.password);
                storage_pass_wifi((char *)wifi_config.sta.ssid, (char *)wifi_config.sta.password);
                err = ESP_OK;
            }
            else if (bits & WIFI_FAIL_BIT)
            {
                ESP_LOGW(TAG, "Failed to connect all WIFI");
                err = ESP_FAIL;
            }
            else
            {
                ESP_LOGE(TAG, "UNEXPECTED EVENT IN");
                err = ESP_FAIL;
            }
        }
    }
    else
    {
        err = ESP_FAIL;
        ESP_LOGE(TAG, "UNEXPECTED EVENT OUT");
    }
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
    return err;
}

void wifiScan()
{
    wifi_scan_config_t scanConf = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false};
    esp_wifi_scan_start(&scanConf, true);
}
void set_wifi_ssid(char *ssdi)
{
    if (ssdi != NULL)
    {
        memcpy(wifi_config.sta.ssid, ssdi, BUF_SIZE_32);
    }
}
void set_wifi_password(char *pass)
{
    memcpy(wifi_config.sta.password, pass, BUF_SIZE_64);
}
bool connect_wifi()
{
    ESP_LOGI(TAG, "Checking to ap Wifi ssid:%s password:%s",
             wifi_config.sta.ssid, wifi_config.sta.password);
    return storage_pass_wifi((char *)wifi_config.sta.ssid, (char *)wifi_config.sta.password);
    /*
    esp_wifi_disconnect();
    s_retry_num = 0;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_stop());
    esp_wifi_start();
    s_wifi_event_group = xEventGroupCreate();
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT)
    {
        // If the connection is successful, save the ssid and password into NSV
        ESP_LOGI(TAG, "Connected to ap Wifi AP:%s password:%s. Storage the ssid and password into NSV",
                 wifi_config.sta.ssid, wifi_config.sta.password);
        storage_pass_wifi((char *)wifi_config.sta.ssid, (char *)wifi_config.sta.password);
        return true;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGW(TAG, "Failed to connect to Wifi AP:%s, password:%s",
                 wifi_config.sta.ssid, wifi_config.sta.password);
        return false;
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
    return false;
    */
}