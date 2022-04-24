#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <string.h>
#define BUF_SIZE (4096)
int size(char* str);
char *getResponse(char *reponseCheckSucess,
                         char *reponseCheckFail,
                         uint32_t timeout,
                         bool *isSuccess);
//#include "network.h"
/**
 *
 *
 UART 0: Debug and connect to raspberry pi
 UART 1: connect to module GPRS
 */

#define AT_SIZE  1024
#define TXD  (GPIO_NUM_16)
#define RXD  (GPIO_NUM_17)
#define RTS  (UART_PIN_NO_CHANGE)
#define CTS  (UART_PIN_NO_CHANGE)

#define GPIO_OUTPUT_IO_0    25
#define GPIO_OUTPUT_IO_1    22
#define GPIO_OUTPUT_IO_2    27
#define GPIO_OUTPUT_PIN_SEL  (1ULL<<GPIO_OUTPUT_IO_0 | 1ULL<<GPIO_OUTPUT_IO_1| 1ULL<<GPIO_OUTPUT_IO_2)


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

//#define _DEBUG_

struct data_receive{
    char data[BUF_SIZE];
    uint16_t len;
};
bool exit_all = false;
char g_user_input_str[AT_SIZE] = {0};
void gps_power(bool value)
{
    // GPIO_OUTPUT_IO_2
    gpio_pad_select_gpio(GPIO_NUM_27);
    gpio_set_direction(GPIO_NUM_27, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_27, value);
    vTaskDelay(1000/portTICK_PERIOD_MS);
}

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
static void print_menu(){
	printf("######################################\n");
	printf("######## NBIOT/GPS AT COMMAND ########\n");
	printf("######################################\n");
}

static void task_read_user_input(void *pvParameters){
	xSemaphoreHandle *mutex = (xSemaphoreHandle *) pvParameters;

	print_menu();
	vTaskDelay(100/portTICK_PERIOD_MS);

	/* Read input from user here */
	char uart_user_input_data[AT_SIZE] = {0};
	int pos = 0;
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
			uart_user_input_data[pos++] = c;
		}

		else if((c == '\n') ||(c== '\r')){

		    if(strlen(uart_user_input_data) > 0){
	            //memcpy(g_user_input_str, data, pos);
		        memset(g_user_input_str , 0x00, sizeof(g_user_input_str));
		        strcpy(g_user_input_str , uart_user_input_data);
	            xSemaphoreGive(*mutex);
	            vTaskDelay(200/portTICK_PERIOD_MS);

	            start_recv_data = false;
	            memset(uart_user_input_data,0x00, sizeof(uart_user_input_data));
	            pos = 0;
		    }
		}

		vTaskDelay(10/portTICK_PERIOD_MS);
	}
}

static void task_send_at_command(void *pvParameters){

	char at_cmd[AT_SIZE] = {0};
	strcpy(at_cmd, pvParameters);

	PRINT_CMD_START(at_cmd, 0);

	int len = strlen(at_cmd);
	at_cmd[len] = '\r';
	at_cmd[len+1] = '\n';
	uart_write_bytes(UART_NUM_2, at_cmd, strlen(at_cmd));

	vTaskDelete(NULL);
}

bool has_carriage_return(char* pData, int len){
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

	char response[AT_SIZE];
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

static void task_handle_user_input(void *pvParameters){
	xSemaphoreHandle *mutex = (xSemaphoreHandle *) pvParameters;

	char input[AT_SIZE]= {0};
	TaskHandle_t at_command_handle = NULL;

	vTaskDelay(200/portTICK_PERIOD_MS);
	while(exit_all == false){

		xSemaphoreTake(*mutex, portMAX_DELAY );
		strcpy(input, g_user_input_str);
		xSemaphoreGive(*mutex);

		/* Handle at command */
		if((strncmp("at", input, strlen("at"))==0) ||
			(strncmp("AT", input, strlen("AT"))==0))
		{
		    char at_cmd[AT_SIZE] = {0};
			strcpy(at_cmd, input);
			xTaskCreate(&task_send_at_command, "handle_at_command", 8196, &at_cmd, 7, at_command_handle);
		}

		else if((strncmp("reboot", input, strlen("reboot"))==0) ||
			(strncmp("REBOOT", input, strlen("REBOOT"))==0))
		{
			esp_restart();
		}

		memset(input, 0x00, sizeof(input));
		vTaskDelay(500/portTICK_PERIOD_MS);
	}

}
static void test_task(void *param){
	while(1){
		printf("Hello RTOS.....\n");
		vTaskDelay(500/portTICK_PERIOD_MS);
	}
}

void app_main(void)
{

	//Init uart between esp32 <--> nbiot
    gps_power(1);
	init_uart_nbiot();

	TaskHandle_t read_user_input_handle = NULL;
	TaskHandle_t handle_user_input_handle = NULL;
	TaskHandle_t handle_recv_at_response = NULL;
	xSemaphoreHandle mutex_data_available   = xSemaphoreCreateMutex();
	xTaskCreate(&task_read_user_input,   "read_user_input",   10240, &mutex_data_available, 6, read_user_input_handle);
	xTaskCreate(&task_handle_user_input, "handle_user_input", 8196, &mutex_data_available, 5, handle_user_input_handle);
	xTaskCreate(&task_recv_at_response,  "recv_at_response",  8196, NULL, 4, handle_recv_at_response);

	//TaskHandle_t test_handle = NULL;
	//xTaskCreate(&test_task,  "test_task", 8096, NULL, 5, test_handle);
	while(1){
		vTaskDelay(100/portTICK_PERIOD_MS);
	}
}
