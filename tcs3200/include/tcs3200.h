#ifndef __TCS3200_H__
#define __TCS3200_H__

#include <stdint.h>

typedef struct {
    uint16_t r;
    uint16_t g;
    uint16_t b;
} tcs3200_raw_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} tcs3200_rgb_t;

typedef enum {
    COLOR_UNKNOWN = 0,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_WHITE,
    COLOR_BLACK,
    COLOR_YELLOW
} tcs3200_color_t;

void tcs3200_init(void);

void tcs3200_calibrate_white(void);
void tcs3200_calibrate_black(void);

void tcs3200_read_raw(tcs3200_raw_t *raw);
void tcs3200_get_rgb(tcs3200_rgb_t *rgb);
tcs3200_color_t tcs3200_get_color(void);

#endif