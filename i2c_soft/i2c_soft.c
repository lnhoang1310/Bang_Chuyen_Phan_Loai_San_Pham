#include "i2c_soft.h"


void i2c_soft_delay(void){
    for(volatile uint32_t i = 0; i < I2C_SOFT_DELAY_US; i++);
}

static void i2c_soft_scl_high(i2c_soft_typedef *i2c){
    gpio_set_level(i2c->SCL_Pin, 1);
}

static void i2c_soft_scl_low(i2c_soft_typedef *i2c){
    gpio_set_level(i2c->SCL_Pin, 0);
}

static void i2c_soft_sda_high(i2c_soft_typedef *i2c){
    gpio_set_level(i2c->SDA_Pin, 1);
}

static void i2c_soft_sda_low(i2c_soft_typedef *i2c){
    gpio_set_level(i2c->SDA_Pin, 0);
}

static bool i2c_soft_sda_read(i2c_soft_typedef *i2c){
    return gpio_get_level(i2c->SDA_Pin) == 1;
}

static void i2c_soft_start(i2c_soft_typedef *i2c){
    i2c_soft_sda_high(i2c);
    i2c_soft_scl_high(i2c);
    i2c_soft_delay();
    i2c_soft_sda_low(i2c);
    i2c_soft_delay();
    i2c_soft_scl_low(i2c);
}

static void i2c_soft_stop(i2c_soft_typedef *i2c){
    i2c_soft_sda_low(i2c);
    i2c_soft_scl_high(i2c);
    i2c_soft_delay();
    i2c_soft_sda_high(i2c);
    i2c_soft_delay();
}

static bool i2c_soft_write_byte(i2c_soft_typedef *i2c, uint8_t byte){
    for(int i = 7; i >= 0; i--){
        if(byte & (1 << i)){
            i2c_soft_sda_high(i2c);
        }else{
            i2c_soft_sda_low(i2c);
        }
        i2c_soft_delay();
        i2c_soft_scl_high(i2c);
        i2c_soft_delay();
        i2c_soft_scl_low(i2c);
    }
    i2c_soft_sda_high(i2c);
    i2c_soft_delay();
    i2c_soft_scl_high(i2c);
    i2c_soft_delay();
    bool ack = !i2c_soft_sda_read(i2c);
    i2c_soft_scl_low(i2c);
    return ack;
}

static uint8_t i2c_soft_read_byte(i2c_soft_typedef *i2c, bool ack){
    uint8_t byte = 0;
    i2c_soft_sda_high(i2c);
    for(int i = 7; i >= 0; i--){
        i2c_soft_delay();
        i2c_soft_scl_high(i2c);
        i2c_soft_delay();
        if (i2c_soft_sda_read(i2c)){
            byte |= (1 << i);
        }
        i2c_soft_scl_low(i2c);
    }
    if(ack){
        i2c_soft_sda_low(i2c);
    }else{
        i2c_soft_sda_high(i2c);
    }
    i2c_soft_delay();
    i2c_soft_scl_high(i2c);
    i2c_soft_delay();
    i2c_soft_scl_low(i2c);
    return byte;
}

void i2c_soft_init(i2c_soft_typedef *i2c, gpio_num_t scl_pin, gpio_num_t sda_pin){
    i2c->SCL_Pin = scl_pin;
    i2c->SDA_Pin = sda_pin;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << i2c->SCL_Pin) | (1ULL << i2c->SDA_Pin), // Chọn GPIO4
        .mode = GPIO_MODE_OUTPUT_OD,            // Đặt chế độ đầu ra
        .pull_up_en = GPIO_PULLUP_ENABLE,   // Không bật pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // Không bật pull-down
        .intr_type = GPIO_INTR_DISABLE       // Không bật ngắt
    };
    gpio_config(&io_conf);

    i2c_soft_sda_high(i2c);
    i2c_soft_scl_high(i2c);
}

bool i2c_soft_read_addr8_data8(i2c_soft_typedef *i2c, uint8_t slave_addr, uint8_t reg_addr, uint8_t *data){
    i2c_soft_start(i2c);
    if (!i2c_soft_write_byte(i2c, (slave_addr << 1) | 0x00))
        return false;
    if (!i2c_soft_write_byte(i2c, reg_addr))
        return false;
    i2c_soft_start(i2c);
    if (!i2c_soft_write_byte(i2c, (slave_addr << 1) | 0x01))
        return false;
    *data = i2c_soft_read_byte(i2c, false);
    i2c_soft_stop(i2c);
    return true;
}

bool i2c_soft_write_addr8_data8(i2c_soft_typedef *i2c, uint8_t slave_addr, uint8_t reg_addr, uint8_t data){
    i2c_soft_start(i2c);
    if (!i2c_soft_write_byte(i2c, (slave_addr << 1) | 0x00))
        return false;
    if (!i2c_soft_write_byte(i2c, reg_addr))
        return false;
    if (!i2c_soft_write_byte(i2c, data))
        return false;
    i2c_soft_stop(i2c);
    return true;
}

bool i2c_soft_read_addr8_data16(i2c_soft_typedef *i2c, uint8_t slave_addr, uint8_t reg_addr, uint16_t *data){
    uint8_t buf[2];
    i2c_soft_start(i2c);
    if (!i2c_soft_write_byte(i2c, (slave_addr << 1) | 0x00))
        return false;
    if (!i2c_soft_write_byte(i2c, reg_addr))
        return false;
    i2c_soft_start(i2c);
    if (!i2c_soft_write_byte(i2c, (slave_addr << 1) | 0x01))
        return false;
    buf[0] = i2c_soft_read_byte(i2c, true);
    buf[1] = i2c_soft_read_byte(i2c, false);
    i2c_soft_stop(i2c);
    *data = (buf[0] << 8) | buf[1];
    return true;
}

bool i2c_soft_read_addr8_data32(i2c_soft_typedef *i2c, uint8_t slave_addr, uint8_t reg_addr, uint32_t *data){
    uint8_t buf[4];
    i2c_soft_start(i2c);
    if (!i2c_soft_write_byte(i2c, (slave_addr << 1) | 0x00))
        return false;
    if (!i2c_soft_write_byte(i2c, reg_addr))
        return false;
    i2c_soft_start(i2c);
    if (!i2c_soft_write_byte(i2c, (slave_addr << 1) | 0x01))
        return false;
    buf[0] = i2c_soft_read_byte(i2c, true);
    buf[1] = i2c_soft_read_byte(i2c, true);
    buf[2] = i2c_soft_read_byte(i2c, true);
    buf[3] = i2c_soft_read_byte(i2c, false);
    i2c_soft_stop(i2c);
    *data = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    return true;
}

bool i2c_soft_read_addr8_bytes(i2c_soft_typedef *i2c, uint8_t slave_addr, uint8_t reg_addr, uint8_t *data, uint8_t len){
    i2c_soft_start(i2c);
    if (!i2c_soft_write_byte(i2c, (slave_addr << 1) | 0x00))
        return false;
    if (!i2c_soft_write_byte(i2c, reg_addr))
        return false;
    i2c_soft_start(i2c);
    if (!i2c_soft_write_byte(i2c, (slave_addr << 1) | 0x01))
        return false;
    for (uint8_t i = 0; i < len - 1; i++){
        data[i] = i2c_soft_read_byte(i2c, true);
    }
    data[len - 1] = i2c_soft_read_byte(i2c, false);
    i2c_soft_stop(i2c);
    return true;
}

bool i2c_soft_write_addr8_bytes(i2c_soft_typedef *i2c, uint8_t slave_addr, uint8_t reg_addr, uint8_t *data, uint8_t len){
    i2c_soft_start(i2c);
    if (!i2c_soft_write_byte(i2c, (slave_addr << 1) | 0x00))
        return false;
    if (!i2c_soft_write_byte(i2c, reg_addr))
        return false;
    for (uint8_t i = 0; i < len; i++){
        if (!i2c_soft_write_byte(i2c, data[i]))
            return false;
    }
    i2c_soft_stop(i2c);
    return true;
}
