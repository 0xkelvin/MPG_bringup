#ifndef __SENSOR_H__
#define __SENSOR_H__
#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "sdkconfig.h"

#include "driver/gpio.h"
#include "esp_adc_cal.h"
#include <math.h>
#include "driver/rtc_io.h"
#include "soc/rtc.h"
#include "Icm20789Defs.h"
#include "hal/i2c_types.h"
#define POWER_SENSOR_ON_OFF 26
#define Temperature_Address 0x48
#define Temperature_Configration_Registor_Addr 0x01
#define Temperature_Registor_Addr 0x00
#define SDA_Pin 19
#define SCL_Pin 21
#define I2C_Port 1
#define WRITE_BIT I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ   /*!< I2C master read */
#define ACK_CHECK_EN 0x1           /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0          /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                /*!< I2C ack value */
#define NACK_VAL 0x1               /*!< I2C nack value */
#define ICM_20789_Address 0x68
#define Test_Reg 0x75

typedef struct accelermeter
{
    float gyroX;
    float gyroY;
    float gyroZ;
    float accX;
    float accY;
    float accZ;
    float temp;
} accelermeter;

void i2c_setup();
esp_err_t i2c_write(i2c_port_t num_, uint8_t addr_, uint8_t reg_, uint8_t data_);
esp_err_t i2c_read(i2c_port_t num_, uint8_t addr_, uint8_t reg_, uint8_t *out_, int len_);
float getTemperatureAT30();
struct accelermeter getMotion();
#endif