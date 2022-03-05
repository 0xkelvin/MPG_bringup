#ifndef __DEFINE_H__
#define __DEFINE_H__
//	pin define
#define voltageSensor 34     //	adc
#define voltageTempSensor 35 //	adc

#define mcuSCL 21  //	i2c
#define mcuSDA 19  //	i2c
#define I2C_Port 1 //	i2c port no

#define buttonScanDevice 14 //	input
#define chargeDetect 2      //	input
#define icm20789Int 12      //	input
#define me310SLedIn 5       //	input
#define me310SPwrMon 4      //	input

#define buzzer 23           //	output
#define gpsOnOff 27         //	output
#define ledDev2 15          //	output
#define ledDev1 13          //	output
#define mcuOnOffWake4g 22   //	output
#define powerSensorOnOff 26 //	output
#define power4gOnOff 25     //	output

#define TIMEOUT 50 // 2.5s
#define COUNT_TIMEOUT  100 //100s Original: //500 // 2.5s
#define ON true
#define OFF false

#define CONNECTED 1
#define DISCONNECTED 0

#define ENABLE 1
#define DISABLE 0

#define NO_SIGNAL 0
#define WEAK_SIGNAL 1
#define MEDIUM_SIGNAL 2
#define STRONG_SIGNAL 3

#define NOT_REGISTERED 0
#define REGISTERED 1
#define REGISTERING 2
#define REGISTERED_DENIED 3
#define UNKNOWN 4
#define ROAMING 5
// Define storage NSV
#define CONFIG_STORAGE "config"
#define KEY_MQTT_SERVER "mqttsvr"
#define KEY_MQTT_PORT "mqttport"
#define KEY_MQTT_TOPIC "mqtttopic"
#define KEY_MQTT_USER "mqttusr"
#define KEY_MQTT_PWD "mqttpwd"
#define KEY_WIFI_HOST "wifihost"
#define KEY_WIFI_PWD "wifipwd"
#define KEY_WIFI_MODE "wifimode"
#define KEY_NBIOT_MODE "nbiotmode"
#define KEY_MODE_POWER "modepwr"
#define DATA_MAIN_LENGTH 6
#define DATA_OFSET 3
#define KEY_MPG_INIT "mpginit"
#define IS_GPS_INIT "configgps"
#define STORAGE_LIST_WIFI "storagelistwifi"
//
#define TXD (GPIO_NUM_16)
#define RXD (GPIO_NUM_17)
#define RTS (UART_PIN_NO_CHANGE)
#define CTS (UART_PIN_NO_CHANGE)
#define POWER_SENSOR_ON_OFF 26
#define GPIO_OUTPUT_IO_0 25
#define GPIO_OUTPUT_IO_1 22
#define GPIO_OUTPUT_IO_2 27
#define GPIO_OUTPUT_PIN_SEL (1ULL << GPIO_OUTPUT_IO_0 | 1ULL << GPIO_OUTPUT_IO_1 | 1ULL << GPIO_OUTPUT_IO_2)

#define EX_UART_NUM UART_NUM_2
#define PATTERN_CHR_NUM (3) /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/
// define buffer size
#define BUF_SIZE_32 (32)
#define BUF_SIZE_64 (64)
#define BUF_SIZE_128 (128)
#define BUF_SIZE_512 (512)
#define BUF_SIZE_1024 (1024)
#define RD_BUF_SIZE (BUF_SIZE_1024)

#define SUBSCRIBER 0
#define PUBLISHER 1

#define WWAN 1
#define GNSS 0

#define MCU_TOPIC "petcare/server/sensor"
#define HEALTH_TOPIC "petcare/server/health" //"petcare/server/temperature"
#define GPS_TOPIC "petcare/server/gps"

#define MODE_TOPIC "petcare/client/%s/mode"
#define MPG_INIT_TOPIC "petcare/server/init"

#define MODE_REALTIME 0
#define MODE_FAST 5       // 1 minute
#define MODE_STANDARD 15  // 5 minute
#define MODE_ULP_SLEEP 60 // 1 h
// OTA update

#define HASH_LEN 32 /* SHA-256 digest length */
#define HOST_NAME "MPG-Monitor:"
#define LEDC_LS_TIMER LEDC_TIMER_1
#define LEDC_LS_MODE LEDC_LOW_SPEED_MODE
#define LEDC_HS_TIMER LEDC_TIMER_0
#define LEDC_HS_MODE LEDC_HIGH_SPEED_MODE

#define LEDC_HS_CH0_CHANNEL LEDC_CHANNEL_0
#define COUNT_TIME_OUT 100
#endif
