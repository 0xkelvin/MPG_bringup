#include <stdio.h>
#include <time.h>
#include "init.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "network.h"
#include "esp_system.h"
#include "define.h"
#include "ota.h"
#include "adc.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "connectWifi.h"
#include "gatt_server.h"
#include "cJSON.h"
#include "mqtt_client.h"
#include "sensor.h"
#include "storage.h"
#include "iot_button.h"
#include "esp_sntp.h"
#include "http_client.h"
#include "common.h"

#include "esp_wifi.h"

//#define TXD  (GPIO_NUM_16)
//#define RXD  (GPIO_NUM_17)
//#define RTS  (UART_PIN_NO_CHANGE)
//#define CTS  (UART_PIN_NO_CHANGE)
//#define GPIO_OUTPUT_IO_0    25
//#define GPIO_OUTPUT_IO_1    22
//#define GPIO_OUTPUT_IO_2    27
//#define GPIO_OUTPUT_PIN_SEL  (1ULL<<GPIO_OUTPUT_IO_0 | 1ULL<<GPIO_OUTPUT_IO_1| 1ULL<<GPIO_OUTPUT_IO_2)
#define BUF_SIZE                (4096)
#define MAX_USER_INPUT_LEN      (512)
#define PRINT_CMD_START(cmd, padding) \
{\
        int num = ((40 - strlen(cmd) + padding ) / 2);\
        printf("%.*s", num, "=================");\
        printf("%s", cmd);\
        printf("%.*s\n", num, "=================");\
}
#define PRINT_CMD_END(cmd, padding) \
{\
        int num = ((40 - strlen(cmd) + padding ) / 2);\
        printf("%.*s", num, "=================");\
        printf("%.*s", strlen(cmd) - padding, "=================");\
        printf("%.*s\n", num, "=================");\
}

bool exit_all = false;
char g_user_input_str[MAX_USER_INPUT_LEN] = {0};

static void print_menu();

/* #################### LED TESTING APP ##################################### */
void app_test_led(){
    //Test for LEDs

    ESP_LOGW("[TestFW][LED]", "Turning on RED  Led  !");
    led_light_on_off(GPIO_LED_RED, ON);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    led_light_on_off(GPIO_LED_RED, OFF);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    ESP_LOGW("[TestFW][LED]", "Turning on BLUE Led  !");
    led_light_on_off(GPIO_LED_BLU, ON);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    led_light_on_off(GPIO_LED_BLU, OFF);
    vTaskDelay(500 / portTICK_PERIOD_MS);

}

/* #################### NBIoT/GPS TESTING APP ############################### */

static void task_send_at_command(void *pvParameters){

    char at_cmd[50] = {0};
    strcpy(at_cmd, pvParameters);

    PRINT_CMD_START(at_cmd, 0);

    int len = strlen(at_cmd);
    at_cmd[len] = '\r';
    at_cmd[len+1] = '\n';
    uart_write_bytes(UART_NUM_2, at_cmd, strlen(at_cmd));

    vTaskDelete(NULL);
}
static bool has_carriage_return(char* pData, int len){
    while(len > 0){
        if((pData[len - 1] == '\r') || (pData[len - 1] == '\n')){
            return true;
        }
        else{
            len--;
        }
    }

    return false;
}
static void task_recv_at_response(void* pvParameters){

    char response[1024];
    int  wait_ms = 10;

    memset(response, 0, sizeof(response));
    while(1){

        uint16_t total_len = 0;
        while(1){
            int len  = 0;
            len += uart_read_bytes(UART_NUM_2,
                            (uint8_t*)response + total_len,
                            BUF_SIZE, wait_ms / portTICK_RATE_MS);
            if (true == has_carriage_return(response + total_len, len)){
                printf(response);
                memset(response, 0, sizeof(response));
                break;
            }
            else{
                total_len += len;
            }
        }

    }

}
void app_test_nbiot_gps(char* input){
    /* Should be tested with AT command */
    TaskHandle_t at_command_handle = NULL;
    char at_cmd[MAX_USER_INPUT_LEN];
    strcpy(at_cmd, input);
    xTaskCreate(&task_send_at_command, "handle_at_command", 8096, &at_cmd, 5, at_command_handle);
}

/* #################### I2C SENSORs TESTING APP ############################# */
void app_test_i2c_sensors(){

    struct accelermeter accel_value = {0};
    accel_value = getMotion();
    ESP_LOGW("[TestFW][I2C]", "Accel(X,Y,Z)    = (%05.2f, %05.2f, %05.2f)", accel_value.accX, accel_value.accY, accel_value.accZ);
    ESP_LOGW("[TestFW][I2C]", "Gyro (X,Y,Z)    = (%05.2f, %05.2f, %05.2f)", accel_value.gyroX, accel_value.gyroY, accel_value.gyroZ);

    float temp_value = {0};
    temp_value = getTemperatureAT30();
    ESP_LOGW("[TestFW][I2C]", "Temperature     = (%2.2f)", temp_value);
}

/* #################### ADC SENSORs TESTING APP ############################# */
void app_test_adc_sensors(){

    float temp_value = getTemperature();
    ESP_LOGW("[TestFW][ADC]", "Temperature     = (%2.2f)", temp_value);

    float volate_value = readVoltage();
    ESP_LOGW("[TestFW][ADC]", "Voltage         = (%2.2f)", volate_value);


    float battery_value = getBattery();
    ESP_LOGW("[TestFW][BAT]", "Battery percent = (%2.2f)", battery_value);

}

/* #################### WIFI TESTING APP #################################### */
static const char* wifi_test_ssid = "SINGTEL-F5E8(2.4G)";
static const char* wifi_test_pswd = "wobouchogu";

extern void app_test_wifi_common_init();
extern void app_test_wifi_scan_init(void);
extern void app_test_wifi_scan(void);
extern void app_test_wifi_scan_deinit(void);

extern void app_test_wifi_station_init(void);
extern void app_test_wifi_station(const char*, const char*);
extern void app_test_wifi_station_deinit(void);

static void task_test_wifi_scan(void *pvParameters){

    xSemaphoreHandle *mutex = (xSemaphoreHandle *) pvParameters;
    xSemaphoreTake(*mutex, portMAX_DELAY );

    ESP_LOGW("[TestFW][WIFI]", "---------------- WIFI Scanning started ----------------");
    app_test_wifi_scan_init();
    app_test_wifi_scan();
    vTaskDelay(1000/portTICK_PERIOD_MS);
    app_test_wifi_scan_deinit();
    ESP_LOGW("[TestFW][WIFI]", "---------------- WIFI Scanning completed --------------");

    xSemaphoreGive(*mutex);
    vTaskDelay(100/portTICK_PERIOD_MS);
    vTaskDelete(NULL);
}
static void task_test_wifi_stat(void *pvParameters){
    xSemaphoreHandle *mutex = (xSemaphoreHandle *) pvParameters;
    xSemaphoreTake(*mutex, portMAX_DELAY );

    ESP_LOGW("[TestFW][WIFI]", "---------------- WIFI Connecting started --------------");
    app_test_wifi_station_init();
    app_test_wifi_station(wifi_test_ssid, wifi_test_pswd);
    app_test_wifi_station_deinit();
    ESP_LOGW("[TestFW][WIFI]", "---------------- WIFI Connecting completed ------------");

    xSemaphoreGive(*mutex);
    vTaskDelay(100/portTICK_PERIOD_MS);
    vTaskDelete(NULL);
}
void app_test_wifi(){
    TaskHandle_t task_wifi_scan_handle = NULL;
    TaskHandle_t task_wifi_stat_handle = NULL;
    xSemaphoreHandle mutex_task_finish   = xSemaphoreCreateMutex();

    xTaskCreate(&task_test_wifi_scan, "test_wifi_scan", 8096, &mutex_task_finish, 5, task_wifi_scan_handle);
    vTaskDelay(500/portTICK_PERIOD_MS);
    xSemaphoreTake(mutex_task_finish, portMAX_DELAY );
    xSemaphoreGive(mutex_task_finish);

    xTaskCreate(&task_test_wifi_stat, "test_wifi_stat", 8096, &mutex_task_finish, 5, task_wifi_stat_handle);
    vTaskDelay(500/portTICK_PERIOD_MS);
    xSemaphoreTake(mutex_task_finish, portMAX_DELAY );
    xSemaphoreGive(mutex_task_finish);

}

/* #################### WIFI TESTING APP #################################### */
extern void app_test_ble_beacon_init();
extern void app_test_ble_beacon_advertising();
extern void app_test_ble_beacon_scanning_start();
extern void app_test_ble_beacon_scanning_stop();
extern void app_test_ble_beacon_deinit();

static void app_test_ble_scanning(uint32_t duration_ms){

    app_test_ble_beacon_scanning_start();
    vTaskDelay(duration_ms / portTICK_PERIOD_MS);
    app_test_ble_beacon_scanning_stop();

}
void app_test_ble()
{
    app_test_ble_beacon_init();
    ESP_LOGW("[TestFW][WIFI]", "---------------- BLE Scanning started!!!---------------");
    app_test_ble_scanning(3000);
    ESP_LOGW("[TestFW][WIFI]", "---------------- BLE Scanning stopped!!!---------------");

    app_test_ble_beacon_deinit();
}

/* ########################################################################## */

static void init_uart_nbiot(void){
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_driver_install(UART_NUM_2, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, TXD, RXD, RTS, CTS);

    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    vTaskDelay(1000/portTICK_PERIOD_MS);
    gpio_set_level(GPIO_OUTPUT_IO_0, 1);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    gpio_set_level(GPIO_OUTPUT_IO_1, 1);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    gpio_set_level(GPIO_OUTPUT_IO_2, 0);

}


static void task_handle_user_input(void *pvParameters){
    xSemaphoreHandle *mutex = (xSemaphoreHandle *) pvParameters;

    char input[MAX_USER_INPUT_LEN]= {0};


    vTaskDelay(200/portTICK_PERIOD_MS);
    while(exit_all == false){
        xSemaphoreTake(*mutex, portMAX_DELAY );
        strcpy(input, g_user_input_str);
        memset(g_user_input_str, 0x00, sizeof(g_user_input_str));
        xSemaphoreGive(*mutex);

        /* Handle at command */
        if((strncmp("at", input, strlen("at"))==0) ||
            (strncmp("AT", input, strlen("AT"))==0))
        {
            app_test_nbiot_gps(input);
        }

        else if((strncmp("reboot", input, strlen("reboot"))==0) ||
            (strncmp("REBOOT", input, strlen("REBOOT"))==0))
        {
            esp_restart();
        }
        else if((strncmp("test_led", input, strlen("test_led"))==0) ||
            (strncmp("TEST_LED", input, strlen("TEST_LED"))==0))
        {
            app_test_led();
            vTaskDelay(500 / portTICK_PERIOD_MS);
            print_menu();
        }
        else if((strncmp("test_sensor", input, strlen("test_sensor"))==0) ||
            (strncmp("TEST_I2C", input, strlen("TEST_I2C"))==0))
        {
            app_test_i2c_sensors();
            vTaskDelay(500 / portTICK_PERIOD_MS);
            app_test_adc_sensors();
            vTaskDelay(500 / portTICK_PERIOD_MS);
            print_menu();
        }
        else if((strncmp("test_wifi", input, strlen("test_wifi"))==0) ||
            (strncmp("TEST_WIFI", input, strlen("TEST_WIFI"))==0))
        {
            app_test_wifi();
            vTaskDelay(500 / portTICK_PERIOD_MS);
            print_menu();
        }
        else if((strncmp("test_ble", input, strlen("test_ble"))==0) ||
            (strncmp("TEST_BLE", input, strlen("TEST_BLE"))==0))
        {
            app_test_ble();
            vTaskDelay(500 / portTICK_PERIOD_MS);
            print_menu();
        }

        memset(input, 0x00, sizeof(input));
        vTaskDelay(200/portTICK_PERIOD_MS);
    }

}

static void print_menu(){
    printf("################################################\n");
    printf("#########   MYPETGO TEST FW   ##################\n");
    printf("################################################\n");
    printf("Test LED       : please type \"test_led\"\n");
    printf("Test SENSOR    : please type \"test_sensor\"\n");
    printf("Test WIFI      : please type \"test_wifi\"\n");
    printf("Test Bluetooth : please type \"test_ble\"\n");
    printf("Test AT        : please enter the AT cmd\n");
    printf("################################################\n");

}
static void task_read_user_input(void *pvParameters){
    xSemaphoreHandle *mutex = (xSemaphoreHandle *) pvParameters;

    print_menu();
    vTaskDelay(100/portTICK_PERIOD_MS);

    /* Read input from user here */
    int pos = 0;
    char data[50] = {0};
    char c = '\0';
    bool start_recv_data = false;
    while(exit_all  == false){

        if (start_recv_data == false){
            start_recv_data = true;
            xSemaphoreTake(*mutex,  portMAX_DELAY);
        }

        c = '\0';
        scanf("%c", &c);

        if ((c != '\0') && (c != '\n') && (c != '\r')){
            data[pos++] = c;
        }

        if((c == '\n') ||(c== '\r')){
            /* User input is ready to be processed */
            //xSemaphoreTake(*mutex,  portMAX_DELAY);
            memcpy(g_user_input_str, data, pos);
            xSemaphoreGive(*mutex);

            start_recv_data = false;
            memset(data,0x00, sizeof(data));
            pos = 0;
        }

        vTaskDelay(100/portTICK_PERIOD_MS);
    }
}


void app_main(void){
    powSensor(true);
    i2c_setup();
    adcSetup();
    init_uart_nbiot();
    vTaskDelay(500 / portTICK_PERIOD_MS);

    TaskHandle_t read_user_input_handle = NULL;
    TaskHandle_t handle_user_input_handle = NULL;
    TaskHandle_t handle_recv_at_response = NULL;
    xSemaphoreHandle mutex_data_available   = xSemaphoreCreateMutex();
    xTaskCreate(&task_read_user_input,   "read_user_input",   8096, &mutex_data_available, 6, read_user_input_handle);
    xTaskCreate(&task_handle_user_input, "handle_user_input", 8096, &mutex_data_available, 5, handle_user_input_handle);
    xTaskCreate(&task_recv_at_response,  "recv_at_response",  8096, &mutex_data_available, 5, handle_recv_at_response);

    while(1){
        vTaskDelay(100/portTICK_PERIOD_MS);
    }
}
void app_main_sequence(void) {

    powSensor(true);
    i2c_setup();
    adcSetup();
    init_uart_nbiot();

    vTaskDelay(500 / portTICK_PERIOD_MS);

    //while(1)
    {
        ESP_LOGW("[TestFW]", "=================START==================");

        app_test_led();
        vTaskDelay(500 / portTICK_PERIOD_MS);

        app_test_i2c_sensors();
        vTaskDelay(500 / portTICK_PERIOD_MS);

        app_test_adc_sensors();
        vTaskDelay(500 / portTICK_PERIOD_MS);

        app_test_wifi();
        vTaskDelay(500 / portTICK_PERIOD_MS);

        app_test_ble();
        vTaskDelay(500 / portTICK_PERIOD_MS);

        ESP_LOGW("[TestFW]", "================= END ==================\n");
    }

    while(1){
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

}
/* ########################################################################## */
