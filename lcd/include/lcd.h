#ifndef LCD_H
#define LCD_H

#include "driver/i2c_master.h"

#define LCD_ADDRESS 0x27
typedef struct
{
    i2c_master_dev_handle_t dev_handle;
    uint8_t i2c_addr;
    uint8_t scl_pin;
    uint8_t sda_pin;
} lcd_typedef;

void lcd_init(lcd_typedef *lcd, i2c_port_t i2c_port, uint8_t i2c_addr, uint8_t scl_pin, uint8_t sda_pin);
void lcd_sendchar(lcd_typedef *lcd, char data);
void lcd_sendstring(lcd_typedef *lcd, const char *str);
void lcd_sendnumber(lcd_typedef *lcd, float number);
void lcd_clear(lcd_typedef *lcd);
void lcd_clearline(lcd_typedef *lcd, uint8_t row);
void lcd_setcursor(lcd_typedef *lcd, uint8_t row, uint8_t col);
void lcd_createchar(lcd_typedef *lcd, uint8_t location, uint8_t charmap[]);
#endif