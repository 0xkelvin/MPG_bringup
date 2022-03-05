#include "esp_system.h"
#include "http_client.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "common.h"
#include <math.h>
#include "define.h"
/*
    typedef enum {
    CHIP_ESP32  = 1, //!< ESP32
    CHIP_ESP32S2 = 2, //!< ESP32-S2
    CHIP_ESP32S3 = 4, //!< ESP32-S3
    CHIP_ESP32C3 = 5, //!< ESP32-C3
} esp_chip_model_t;

    typedef struct {
    esp_chip_model_t model;  //!< chip model, one of esp_chip_model_t
    uint32_t features;       //!< bit mask of CHIP_FEATURE_x feature flags
    uint8_t cores;           //!< number of CPU cores
    uint8_t revision;        //!< chip revision number
} esp_chip_info_t;
*/
static const char *TAG = "common";

char *chipInfo(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    char chipInfo_str[BUF_SIZE_128] = {0};
    sprintf(chipInfo_str, "%s chip with %d CPU core(s), WiFi%s%s, revision: %d, flash size: %dMB %s, free heap size: %d bytes",
            CONFIG_IDF_TARGET,
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
            chip_info.revision,
            spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external",
            esp_get_minimum_free_heap_size());
    char *pChar = malloc(sizeof(chipInfo_str));
    memcpy(pChar, chipInfo_str, strlen(chipInfo_str) + 1);
    return pChar;
}
// input default value, if get value error, or not exist then return default value
esp_err_t get_value_nsv(char *key, uint8_t *out_value)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    // Open
    nvs_handle_t sensorNvs;
    err = nvs_open(CONFIG_STORAGE, NVS_READONLY, &sensorNvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle. (%s)", esp_err_to_name(err), key);
        return err;
    }
    else
    {
        err = nvs_get_u8(sensorNvs, key, out_value);
        nvs_close(sensorNvs);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (%s) Can't get parameter (%s)", esp_err_to_name(err), key);
            return err;
        }
    }
    return err;
}
bool set_value_nsv(char *mode, uint8_t value)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    nvs_handle_t sensorNvs;
    err = nvs_open(CONFIG_STORAGE, NVS_READWRITE, &sensorNvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle. (%s=%d)\n", esp_err_to_name(err), mode, value);
        return false;
    }
    err = nvs_set_u8(sensorNvs, mode, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed store into memory %s", mode);
        return false;
    }
    err = nvs_commit(sensorNvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed NVS commit memory %s", mode);
        return false;
    }
    fflush(stdout);
    nvs_close(sensorNvs);
    return true;
}

void led_light_on_off(uint8_t gpioNumber, bool value)
{
    gpio_pad_select_gpio(gpioNumber);
    gpio_set_direction(gpioNumber, GPIO_MODE_OUTPUT);
    gpio_set_level(gpioNumber, value);
}

void buzzeron(void *arg)
{
    int ch;
    bool on_off = (bool)arg;
    gpio_pad_select_gpio(POWER_SENSOR_ON_OFF);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(POWER_SENSOR_ON_OFF, GPIO_MODE_OUTPUT);
    gpio_set_level(POWER_SENSOR_ON_OFF, on_off);
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
        .freq_hz = 4020,                      // frequency of PWM signal
        .speed_mode = LEDC_LS_MODE,           // timer mode
        .timer_num = LEDC_LS_TIMER,           // timer index
        .clk_cfg = LEDC_AUTO_CLK,             // Auto select the source clock
    };
    // Set configuration of timer0 for high speed channels
    ledc_timer_config(&ledc_timer);

    // Prepare and set configuration of timer1 for low speed channels
    ledc_timer.speed_mode = LEDC_HS_MODE;
    ledc_timer.timer_num = LEDC_HS_TIMER;
    ledc_timer_config(&ledc_timer);
    ledc_channel_config_t ledc_channel[1] = {
        {.channel = LEDC_HS_CH0_CHANNEL,
         .duty = 0,
         .gpio_num = 23,
         .speed_mode = LEDC_HS_MODE,
         .hpoint = 0,
         .timer_sel = LEDC_HS_TIMER}};

    // Set LED Controller with previously prepared configuration
    for (ch = 0; ch < 1; ch++)
    {
        ledc_channel_config(&ledc_channel[ch]);
    }

    // Initialize fade service.
    ledc_fade_func_install(0);
    for (int i = 0; i < 5; i++)
    {
        for (ch = 0; ch < 1; ch++)
        {
            ledc_set_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel, 1000);
            ledc_update_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel);
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
        for (ch = 0; ch < 1; ch++)
        {
            ledc_set_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel, 0);
            ledc_update_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel);
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
        for (ch = 0; ch < 1; ch++)
        {
            ledc_set_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel, 1000);
            ledc_update_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel);
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
        for (ch = 0; ch < 1; ch++)
        {
            ledc_set_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel, 0);
            ledc_update_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel);
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    led_light_on_off(GPIO_LED_RED, OFF);
    vTaskDelete(NULL);
    ESP_LOGI(TAG, "buzzeron end");
    return;
}
bool storage_pass_wifi(char *ssid, char *password)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    nvs_handle_t sensorNvs;
    err = nvs_open(STORAGE_LIST_WIFI, NVS_READWRITE, &sensorNvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle. ssid:%s pass%s\n", esp_err_to_name(err), ssid, password);
        return false;
    }
    err = nvs_set_str(sensorNvs, ssid, password);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed store into memory %s", ssid);
        return false;
    }
    err = nvs_commit(sensorNvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed NVS commit memory %s", ssid);
        return false;
    }
    fflush(stdout);
    nvs_close(sensorNvs);
    return true;
}
char *read_pass_wifi_by_ssid(char *ssid)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    // Open
    nvs_handle_t sensorNvs;
    err = nvs_open(STORAGE_LIST_WIFI, NVS_READONLY, &sensorNvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle. (%s)", esp_err_to_name(err), ssid);
        return NULL;
    }
    else
    {
        size_t required_size = 0;
        err = nvs_get_str(sensorNvs, ssid, NULL, &required_size);
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGE(TAG, "The value %s is not initialized yet!", ssid);
            return NULL;
        }
        else
        {
            char *out_password = malloc(required_size);
            err = nvs_get_str(sensorNvs, ssid, out_password, &required_size);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Can't read SSID Wifi and pass word");
                return NULL;
            }

            return out_password;
        }
    }
    return NULL;
}
void light_red_blu_mix(void *milliseconds)
{
    int cnt_light = (int)milliseconds;
    int flash_blinks = 500;
    static bool turn = true;
    while (flash_blinks <= cnt_light)
    {
        led_light_on_off(GPIO_LED_BLU, turn);
        led_light_on_off(GPIO_LED_RED, !turn);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        flash_blinks = flash_blinks + 500;
        turn = !turn;
    }
    led_light_on_off(GPIO_LED_BLU, false);
    led_light_on_off(GPIO_LED_RED, false);
    vTaskDelete(NULL);
    return;
}