#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "define.h"
#define AT_COMMAND "AT"
#define CHECK_PROVIDER_COMMAND "AT#SPN"
#define CHECK_SIM_STATUS "AT#QSS?" // - Query SIM Status
#define CHECK_SIGNAL_COMMAND "AT+CSQ"
#define CHECK_DNS_COMMAND "AT#QDNS=\"google.com\""
//#define NETWORK_REGISTRATION_COMMAND "AT+CREG=1\r\n" //for GSM
//#define CHECK_REGISTRATION_COMMAND "AT+CREG?\r\n"    //For GSM
#define NETWORK_REGISTRATION_COMMAND "AT+CEREG=1"      //For 3G/4G
#define CHECK_REGISTRATION_COMMAND "AT+CEREG?"         //For 3G/4G
#define DISABLE_NETWORK_COMMAND "AT+CEREG=0"
//#define CHECK_OPERATORS_COMMAND "AT+COPS?" // to check operators
#define CHECK_OPERATORS_COMMAND "AT+COPS=?" // to check operators

// MQTT AT
#define ACTIVE_PDP_COMMAND "AT#SGACT=1,1"			 // activate the context
#define ENABLE_MQTT_COMMAND "AT#MQEN=%d,%d"			 // enable the MQTT client
#define CONFIG_MQTT_COMMAND "AT#MQCFG=%d,%s,%d,1"	 // Configure MQTT Parameters: the last field set equal to 1 is to enable TLS over MQTT
#define CONFIG_ADD_MQTT_COMMAND "AT#MQCFG2=%d,%d,1"	 // Configure Additional MQTT Parameters
#define CONNECT_MQTT_COMMAND "AT#MQCONN=%d,%s,%s,%s" // Connect and Log in the MQTT Broker: instanceNumber, clientID, userName, passWord
#define CHECK_MQTT_CONNECTED "AT#MQCONN?"
#define DISCONNECT_MQTT_CONNECTED "AT#MQDISC=%d" // Disconnecr mqtt

//#define CONFIG_PDP_COMMAND "AT+CGDCONT=1,\"IP\",\"%s\",\"%s\",0,0\r\n" // APN and IP
#define CONFIG_PDP_COMMAND "AT+CGDCONT=1,\"IP\",\"\",\"\",0,0"
#define DISABLE_PDP_COMMAND "AT#SGACT=1,0\r\n"
//#define CHECK_ADDRESS_COMMAND "AT+CGPADDR=1\r\n"
#define CHECK_ADDRESS_COMMAND "AT+CGCONTRDP=1"
#define GPS_CONFIG "AT$GPSCFG=%d,%d"

#define GPS_GET_CONFIG "AT$GPSCFG?"

#define GPS_POWER "AT$GPSP=%d"
#define CHECK_GPS_POWER "AT$GPSP?"
#define GPS_CONFIG_DATA "AT$GPSNMUN=1,0,0,0,1,1,0"
//<enable>, <GGA>, <GLL>, <GSA>, <GSV>, <RMC>, <VTG>.
//$GPSNMUNEX:<GNGNS>,<GNGSA>,<GLGSV>,<GPGRS>,<GAGSV>,<GAGSA>,<GAVTG>,<GPGGA>,<PQGSA>,<PQGSV>,<GNVTG>,<GNRMC>,<GNGGA>
#define GPS_CONFIG_DATA_EX "AT$GPSNMUNEX=0,0,1,0,0,0,0,0,0,0,0,1,0"
#define GPS_READ_CONFIG "AT$GPSNMUN?"
#define AUTO_GPS_NMUN "$GPSNMUN:"
#define REBOOT "AT#REBOOT"
#define FUNCTION_SAVING "AT+CFUN=5"
#define FUNCTION_GPS "AT+CFUN=4"
#define REVISION_GPS "at+gmr\r\n"
#define POSITION_GPS "AT$GPSACP"
#define AUTO_POSITION_GPS "$GPSACP:"

#define HTTP_CONFIG "AT#HTTPCFG=0,\"pettracker-staging.et.r.appspot.com\",443,0," \
					","                                                           \
					",1,120,1"
#define HTTP_QRY "AT#HTTPQRY=0,0,\"/api/devices/%s/version\""

#define UTC_SET_TYPE "AT#CCLKMODE=1"
#define UTC_GET_TIME "AT+CCLK?"

// Select Wireless Network
#define WIRELESS_NETWWORK_COMMAND "AT+WS46?"
// 12: GSM Digital Cellular Systems, GERAN only
// 28: E-UTRAN only
// 30: GERAN and E-UTRAN
#define LTE_SUPPORT_COMMAND "AT#WS46?"
// LTE technology supported
// 0: CAT-M1
// 1: NB-IoT
// 2: CAT-M1 (preferred) and NB-IoT
// 3: CAT-M1 and NB-IoT (preferred)

enum DataType
{
	TypeNull = 0,
	TypeGPS = 1,
	TypeAT = 2,
	TypeCREG = 4,
	TypeAll = 255
};
enum NetworkStateInit
{
	NB_START = 10,
	NB_RESOLVE_DNS = 11,
	NB_SPN = 12,
	NB_CREG = 13,
	NB_SUCCESS = 14,
	NB_INIT_FALSE = 15,
};

enum NetworkStateConnect
{
	START = 0,
	SPN = 1,
	CSQ = 2,
	CREG = 3,
	ADDR = 4,
	SUCCESS = 5,
	STOP = 6
};

enum GPSMode
{
	GPS_NOTUSE = 0,
	INIT_GPS = 1,
	CONFIG_GPS = 2,
	START_GPS = 3,
	STOP_GPS = 4,
	GPS_NULL = 5

};

enum SerialMode
{
	NOT_USE = 0,
	INIT_NETWORK = 1,
	INITIALIZED_NETWORK = 2,
	CONNECT_NETWORK = 3,
	CONNECTED_NETWORK = 4,
	STOP_NETWORK = 5,
	DATE_TIME_PROVIDER_START = 6,
	DATE_TIME_PROVIDER_END = 7,
	MODE_MQTT_STATR = 8,
	MODE_MQTT_STOP = 9,
	MODE_SEARCH_GPS = 10,
};

enum MqttMode
{
	DISCONNECTED_MQTT = 0,
	DISCONNECT_MQTT = 1,
	INIT_MQTT = 2,
	INITED_MQTT = 3,
	CONNECT_MQTT = 4,
	CONNECTED_MQTT = 5,
	PUBLISH_MQTT = 6,
	SUBSCRIBE_MQTT = 7,
	UNSUBSCRIBE_MQTT = 8,
	UNSUBSCRIBED_MQTT = 9
};

enum MqttConnectState
{
	INITED_AND_NOT_CONNECTED = 0,
	CONNECTED_STATE = 1,
	CONNECTION_CLOSED = 2,
	PING_REQUEST_NOT_RECEIVED = 3,
	CONNACK_NOT_RECEIVED = 4,
	CONNECT_NOT_DELIVERED = 5,
	FAILURE_APIS = 6,
	SOCKET_TIMEOUT = 7
};

struct Mqtt
{
	uint8_t channel;
	uint8_t state;
	char hostname[BUF_SIZE_32];
	uint16_t port;
	uint16_t keepAlive;
	char userName[BUF_SIZE_32];
	char passWord[BUF_SIZE_32];
	enum MqttConnectState connectState;
	enum MqttMode mode;
};
/*
struct data
{
	char data[BUF_SIZE];
	uint32_t size;
	enum DataType type;
};
*/
struct NetworkInfo
{
	bool uartState;
	char provider[BUF_SIZE_32];
	int signalStrength;
	int signalReceived;
	int registrationState;
	int registrationStatus;
	char IPV4[BUF_SIZE_32];
	char APN[BUF_SIZE_32];
	char CCID[BUF_SIZE_32];
	char COPS[BUF_SIZE_64];
};

enum GpsState
{
	GPS_STATE_NULL = 0,
	GPS_STATE_CONFIGURING = 1,
	GPS_STATE_CONFIGURED = 2,
	GPS_STATE_STARTED = 3,
	GPS_STATE_STOPED = 4,
	GPS_STATE_REBOOT = 5
};

struct Gps
{
	char time[17];
	double latitude;
	double longitude;
	double altitude;
	double spkm;
	enum GpsState state;
};

void me310_init(char *mac);
void me310_power(bool value);
void me310_WakeUp4g(bool value);
void gps_power(bool value);
void uart_event_task(void *pvParameters);
// void networkInit(void);
void network_task(void *pvParameters);
void set_serial_mode(enum SerialMode mode);
uint8_t get_serial_mode(void);
struct NetworkInfo get_network_info(void);

void send_command(char *command);
void mqtt_init(struct Mqtt mqtt, uint8_t type);
int mqtt_connect(uint8_t type);
int mqtt_disconnect(uint8_t type);
int mqtt_publish(const char *topic, const char *data);
int mqtt_subscribe(const char *topic, const char topicSize);
size_t dataSize(const char *data);

void set_gps_tbf(int time);

void set_gps_mode(enum GPSMode mode);
struct Gps get_gps(void);
struct tm get_time_provider(void);
void clear_gps_flag(void);
bool get_gps_flag(void);
uint8_t get_mqtt_state(uint8_t type);
#endif
