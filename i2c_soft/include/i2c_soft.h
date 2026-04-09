#ifndef I2C_SOFT_H
#define I2C_SOFT_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

#define I2C_SOFT_DELAY_US 5

typedef struct{
    gpio_num_t SCL_Pin;
    gpio_num_t SDA_Pin;
}i2c_soft_typedef;

void i2c_soft_init(i2c_soft_typedef *i2c, gpio_num_t SCL_Pin, gpio_num_t SDA_Pin);
bool i2c_soft_read_addr8_data8(i2c_soft_typedef* i2c, uint8_t slave_addr, uint8_t reg_addr, uint8_t* data);
bool i2c_soft_write_addr8_data8(i2c_soft_typedef* i2c, uint8_t slave_addr, uint8_t reg_addr, uint8_t data);
bool i2c_soft_read_addr8_data16(i2c_soft_typedef* i2c, uint8_t slave_addr, uint8_t reg_addr, uint16_t* data);
bool i2c_soft_read_addr8_data32(i2c_soft_typedef* i2c, uint8_t slave_addr, uint8_t reg_addr, uint32_t* data);
bool i2c_soft_read_addr8_bytes(i2c_soft_typedef* i2c, uint8_t slave_addr, uint8_t reg_addr, uint8_t* data, uint8_t length);
bool i2c_soft_write_addr8_bytes(i2c_soft_typedef* i2c, uint8_t slave_addr, uint8_t reg_addr, uint8_t* data, uint8_t length);

#endif