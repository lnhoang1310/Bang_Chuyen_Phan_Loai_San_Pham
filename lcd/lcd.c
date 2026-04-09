#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "lcd.h"

static TickType_t ms(uint32_t milliseconds)
{
    return pdMS_TO_TICKS(milliseconds) > 0 ? pdMS_TO_TICKS(milliseconds) : 1;
}

static void send_4bit(lcd_typedef *lcd, uint8_t nibble)
{
    uint8_t data[2];
    data[0] = (nibble & 0xF0) | 0x0C;
    data[1] = (nibble & 0xF0) | 0x08;

    i2c_master_transmit(lcd->dev_handle, data, 2, -1);
}

static void send_command(lcd_typedef *lcd, uint8_t cmd)
{
    uint8_t data[4];
    data[0] = (cmd & 0xF0) | 0x0C;
    data[1] = (cmd & 0xF0) | 0x08;
    data[2] = ((cmd << 4) & 0xF0) | 0x0C;
    data[3] = ((cmd << 4) & 0xF0) | 0x08;
    i2c_master_transmit(lcd->dev_handle, data, 4, -1);
}

static void send_data(lcd_typedef *lcd, uint8_t data)
{
    uint8_t packet[4];
    packet[0] = (data & 0xF0) | 0x0D;
    packet[1] = (data & 0xF0) | 0x09;
    packet[2] = ((data << 4) & 0xF0) | 0x0D;
    packet[3] = ((data << 4) & 0xF0) | 0x09;
    i2c_master_transmit(lcd->dev_handle, packet, 4, -1);
}

void lcd_init(lcd_typedef *lcd, i2c_port_t i2c_port, uint8_t i2c_addr, uint8_t scl_pin, uint8_t sda_pin)
{
    lcd->i2c_addr = i2c_addr;
    lcd->scl_pin = scl_pin;
    lcd->sda_pin = sda_pin;

    i2c_master_bus_config_t conf = {
        .i2c_port = i2c_port,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .scl_io_num = scl_pin,
        .sda_io_num = sda_pin,
        .flags.enable_internal_pullup = true,
        .glitch_ignore_cnt = 7};
    i2c_master_bus_handle_t bus_handle;
    if (i2c_new_master_bus(&conf, &bus_handle) == ESP_OK)
    {
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = i2c_addr,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &lcd->dev_handle));

    vTaskDelay(ms(50));
    send_4bit(lcd, 0x30);
    vTaskDelay(ms(5));
    send_4bit(lcd, 0x30);
    vTaskDelay(ms(5));
    send_4bit(lcd, 0x30);
    vTaskDelay(ms(5));
    send_4bit(lcd, 0x20);
    vTaskDelay(ms(10));
    send_command(lcd, 0x28);
    vTaskDelay(ms(5));
    send_command(lcd, 0x08);
    vTaskDelay(ms(5));
    send_command(lcd, 0x01);
    vTaskDelay(ms(5));

    send_command(lcd, 0x06);
    vTaskDelay(ms(5));
    send_command(lcd, 0x0C);
    vTaskDelay(ms(5));
}

void lcd_sendchar(lcd_typedef *lcd, char data)
{
    send_data(lcd, (uint8_t)data);
}

void lcd_sendstring(lcd_typedef *lcd, const char *str)
{
    while (*str)
    {
        lcd_sendchar(lcd, *str++);
    }
}

void lcd_sendnumber(lcd_typedef *lcd, float number)
{
    char buffer[16];
    sprintf(buffer, "%.1f", number);
    lcd_sendstring(lcd, buffer);
}

void lcd_clear(lcd_typedef *lcd)
{
    send_command(lcd, 0x01);
    vTaskDelay(ms(2));
}

void lcd_clearline(lcd_typedef *lcd, uint8_t row)
{
    lcd_setcursor(lcd, row, 0);
    lcd_sendstring(lcd, "                    ");
}

void lcd_setcursor(lcd_typedef *lcd, uint8_t row, uint8_t col)
{
    uint8_t address_offset[] = {0x00, 0x40, 0x14, 0x54};
    if (row >= 4)
        row = 3;
    send_command(lcd, 0x80 | (col + address_offset[row]));
}
