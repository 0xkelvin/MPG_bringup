
#include "sensor.h"
#include "parser.h"
#include "define.h"
#include "esp_log.h"
static const char *TAG = "Sensor";
void i2c_setup()
{

    i2c_config_t conf_i2c;
    conf_i2c.mode = I2C_MODE_MASTER;
    conf_i2c.master.clk_speed = 100000;
    conf_i2c.scl_io_num = SCL_Pin;
    conf_i2c.sda_io_num = SDA_Pin;
    conf_i2c.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf_i2c.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf_i2c.clk_flags = 0;
    i2c_param_config(I2C_Port, &conf_i2c);

    int res = i2c_driver_install(I2C_Port, conf_i2c.mode, 0, 0, 0);
    if (res == ESP_OK)
    {
        ESP_LOGI(TAG, "[ESP32][I2C] Init Success");
    }
    else
    {
        ESP_LOGI(TAG, "[ESP32][I2C] Init Fail");
    }

    i2c_cmd_link_create();
    uint8_t config_data[2];
    config_data[0] = 0x60;
    config_data[1] = 0x00;
    res = i2c_write(I2C_Port, Temperature_Address, Temperature_Configration_Registor_Addr, config_data[0]);
    if (res != ESP_OK)
    {
        ESP_LOGI(TAG, "FAILL Temperature_Configration_Registor_Addr");
    }
    else
    {
        ESP_LOGI(TAG, "Configure Temperature_Configration_Registor_Addr conpleted !!!");
    }
    vTaskDelay(30 / portTICK_RATE_MS);
    uint8_t regPwr1 = BIT_TEMP_DISABLE;
    // write configure ICM-20789 - turn on chip - disable temperature sensor of chip
    res = i2c_write(I2C_Port, DIAMOND_I2C_ADDRESS, REG_PWR_MGMT_1, regPwr1);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILL regPwr1");
    }
    else
    {
        ESP_LOGI(TAG, "Configure ICM-20789 - turn on chip conpleted !!!");
    }
    // write configure register power managerment 2 0 turn off low power | turn off DMP
    uint8_t regPwr2 = BIT_DMP_LP_DIS | BIT_LP_DIS;
    res = i2c_write(I2C_Port, DIAMOND_I2C_ADDRESS, REG_PWR_MGMT_2, regPwr2);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILL regPwr1");
    }
    else
    {
        ESP_LOGI(TAG, "Configure power managerment conpleted !!!");
    }
    vTaskDelay(50 / portTICK_RATE_MS);
}

esp_err_t i2c_write(i2c_port_t num_, uint8_t addr_, uint8_t reg_, uint8_t data_)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, addr_ << 1 | WRITE_BIT, I2C_MASTER_NACK);
    i2c_master_write_byte(cmd, reg_, I2C_MASTER_NACK);
    i2c_master_write_byte(cmd, data_, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(num_, cmd, 1000 / portTICK_RATE_MS);
    if (ret != ESP_OK)
    {
        i2c_cmd_link_delete(cmd);
        return ESP_FAIL;
    }
    i2c_cmd_link_delete(cmd);
    return ESP_OK;
}
esp_err_t i2c_read(i2c_port_t num_, uint8_t addr_, uint8_t reg_, uint8_t *out_, int len_)
{

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    if (addr_ != 0x0)
    {

        i2c_master_write_byte(cmd, addr_ << 1 | WRITE_BIT, ACK_CHECK_EN);
        i2c_master_write_byte(cmd, reg_, ACK_CHECK_EN);
        i2c_master_start(cmd);
    }
    i2c_master_write_byte(cmd, addr_ << 1 | READ_BIT, ACK_CHECK_EN);
    if (len_ > 1)
    {

        i2c_master_read(cmd, out_, len_ - 1, ACK_VAL);
    }
    i2c_master_read_byte(cmd, out_ + len_ - 1, NACK_VAL);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(num_, cmd, 1000 / portTICK_RATE_MS);
    if (ret != ESP_OK)
    {
        i2c_cmd_link_delete(cmd);
        return ESP_FAIL;
    }
    i2c_cmd_link_delete(cmd);
    return ESP_OK;
}
float getTemperatureAT30()
{
    uint8_t read_data[2];
    read_data[0] = 0;
    read_data[1] = 0;
    int res = i2c_read(I2C_Port, Temperature_Address, Temperature_Registor_Addr, read_data, 2);
    if (res == ESP_OK)
    {
        int temp = (read_data[0] * 256 + (read_data[1] & 0xF0)) / 16;
        if (temp > 2047)
        {
            temp -= 4096;
        }
        float cTemp = temp * 0.0625;
        cTemp = cTemp - cTemp * 10 / 100;
        return cTemp;
    }
    else
    {
        ESP_LOGE(TAG, "Read fail: IC2 Temperature_Address");
    }
    return 0;
}
struct accelermeter getMotion()
{
    // int len = 1;
    uint8_t accXL = 0;
    uint8_t accXH = 0;
    uint8_t accYL = 0;
    uint8_t accYH = 0;
    uint8_t accZL = 0;
    uint8_t accZH = 0;
    uint8_t gyroXL = 0;
    uint8_t gyroXH = 0;
    uint8_t gyroYL = 0;
    uint8_t gyroYH = 0;
    uint8_t gyroZL = 0;
    uint8_t gyroZH = 0;
    uint8_t tempL = 0;
    uint8_t tempH = 0;
    int res = 0;
    res = i2c_read(I2C_Port, DIAMOND_I2C_ADDRESS, MPUREG_ACCEL_XOUT_H, &accXH, 1);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILL MPUREG_ACCEL_XOUT_H");
    }

    res = i2c_read(I2C_Port, DIAMOND_I2C_ADDRESS, MPUREG_ACCEL_XOUT_L, &accXL, 1);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILL MPUREG_ACCEL_XOUT_L");
    }

    res = i2c_read(I2C_Port, DIAMOND_I2C_ADDRESS, MPUREG_ACCEL_YOUT_H, &accYH, 1);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILL MPUREG_ACCEL_YOUT_H");
    }

    res = i2c_read(I2C_Port, DIAMOND_I2C_ADDRESS, MPUREG_ACCEL_XOUT_L, &accYL, 1);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILL MPUREG_ACCEL_YOUT_L");
    }

    res = i2c_read(I2C_Port, DIAMOND_I2C_ADDRESS, MPUREG_ACCEL_ZOUT_H, &accZH, 1);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILL MPUREG_ACCEL_ZOUT_H");
    }

    res = i2c_read(I2C_Port, DIAMOND_I2C_ADDRESS, MPUREG_ACCEL_XOUT_L, &accZL, 1);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILL MPUREG_ACCEL_ZOUT_L");
    }

    res = i2c_read(I2C_Port, DIAMOND_I2C_ADDRESS, MPUREG_GYRO_XOUT_H, &gyroXH, 1);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILL MPUREG_ACCEL_XOUT_H");
    }

    res = i2c_read(I2C_Port, DIAMOND_I2C_ADDRESS, MPUREG_GYRO_XOUT_L, &gyroXL, 1);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILL MPUREG_ACCEL_XOUT_L");
    }

    res = i2c_read(I2C_Port, DIAMOND_I2C_ADDRESS, MPUREG_GYRO_YOUT_H, &gyroYH, 1);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILL MPUREG_ACCEL_XOUT_H");
    }

    res = i2c_read(I2C_Port, DIAMOND_I2C_ADDRESS, MPUREG_GYRO_YOUT_L, &gyroYL, 1);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILL MPUREG_ACCEL_XOUT_L");
    }

    res = i2c_read(I2C_Port, DIAMOND_I2C_ADDRESS, MPUREG_GYRO_ZOUT_H, &gyroZH, 1);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILL MPUREG_ACCEL_XOUT_H");
    }

    res = i2c_read(I2C_Port, DIAMOND_I2C_ADDRESS, MPUREG_GYRO_ZOUT_L, &gyroZL, 1);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILL MPUREG_ACCEL_XOUT_L");
    }

    res = i2c_read(I2C_Port, DIAMOND_I2C_ADDRESS, MPUREG_TEMP_XOUT_H, &tempH, 1);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILL MPUREG_TEMP_XOUT_H");
    }

    res = i2c_read(I2C_Port, DIAMOND_I2C_ADDRESS, MPUREG_TEMP_XOUT_L, &tempL, 1);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "FAILL MPUREG_TEMP_XOUT_L");
    }

    struct accelermeter motion = {0};
    motion.gyroX = (float)((gyroXH << 8) | gyroXL);
    motion.gyroY = (float)((gyroYH << 8) | gyroYL);
    motion.gyroZ = (float)((gyroZH << 8) | gyroZL);
    motion.accX = (float)((accXH << 8) | accXL);
    motion.accY = (float)((accYH << 8) | accYL);
    motion.accZ = (float)((accZH << 8) | accZL);
    motion.temp = (float)((tempH << 8) | tempL);
    return motion;
}