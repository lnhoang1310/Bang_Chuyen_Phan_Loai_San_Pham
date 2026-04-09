#include "tcs3200.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#define PIN_OUT 4
#define PIN_S2 18
#define PIN_S3 19
#define PIN_S0 21
#define PIN_S1 22

static pcnt_unit_handle_t pcnt_unit = NULL;
static pcnt_channel_handle_t pcnt_chan = NULL;

static tcs3200_raw_t white_ref = {1, 1, 1};
static tcs3200_raw_t black_ref = {0, 0, 0};

static void pcnt_init_new()
{
    pcnt_unit_config_t unit_config = {
        .high_limit = 30000,
        .low_limit = 0,
    };

    pcnt_new_unit(&unit_config, &pcnt_unit);

    pcnt_chan_config_t chan_config = {
        .edge_gpio_num = PIN_OUT,
        .level_gpio_num = -1,
    };

    pcnt_new_channel(pcnt_unit, &chan_config, &pcnt_chan);

    pcnt_channel_set_edge_action(
        pcnt_chan,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        PCNT_CHANNEL_EDGE_ACTION_HOLD);

    pcnt_unit_enable(pcnt_unit);
    pcnt_unit_clear_count(pcnt_unit);
    pcnt_unit_start(pcnt_unit);
}

static void gpio_init_tcs()
{
    gpio_config_t io = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_S2) | (1ULL << PIN_S3) |
                        (1ULL << PIN_S0) | (1ULL << PIN_S1),
    };
    gpio_config(&io);

    // scale = 20%
    gpio_set_level(PIN_S0, 1);
    gpio_set_level(PIN_S1, 0);
}

static void set_filter(uint8_t s2, uint8_t s3)
{
    gpio_set_level(PIN_S2, s2);
    gpio_set_level(PIN_S3, s3);
}

static uint16_t read_freq()
{
    int count = 0;

    pcnt_unit_clear_count(pcnt_unit);
    vTaskDelay(pdMS_TO_TICKS(20));
    pcnt_unit_get_count(pcnt_unit, &count);

    return count;
}

void tcs3200_init(void)
{
    gpio_init_tcs();
    pcnt_init_new();
}

void tcs3200_read_raw(tcs3200_raw_t *raw)
{
    set_filter(0, 0); // RED
    raw->r = read_freq();

    set_filter(0, 1); // BLUE
    raw->b = read_freq();

    set_filter(1, 1); // GREEN
    raw->g = read_freq();
}

void tcs3200_calibrate_white(void)
{
    vTaskDelay(pdMS_TO_TICKS(200));
    tcs3200_read_raw(&white_ref);
}

void tcs3200_calibrate_black(void)
{
    vTaskDelay(pdMS_TO_TICKS(200));
    tcs3200_read_raw(&black_ref);
}

static uint8_t normalize(uint16_t val, uint16_t min, uint16_t max)
{
    if (max <= min)
        return 0;

    int32_t v = (val - min) * 255 / (max - min);

    if (v < 0)
        v = 0;
    if (v > 255)
        v = 255;

    return (uint8_t)v;
}

void tcs3200_get_rgb(tcs3200_rgb_t *rgb)
{
    tcs3200_raw_t raw;
    tcs3200_read_raw(&raw);

    rgb->r = normalize(raw.r, black_ref.r, white_ref.r);
    rgb->g = normalize(raw.g, black_ref.g, white_ref.g);
    rgb->b = normalize(raw.b, black_ref.b, white_ref.b);
}

tcs3200_color_t tcs3200_get_color(void)
{
    tcs3200_rgb_t rgb;
    tcs3200_get_rgb(&rgb);

    uint16_t sum = rgb.r + rgb.g + rgb.b;
    if (sum == 0)
        return COLOR_BLACK;

    float r_ratio = (float)rgb.r / sum;
    float g_ratio = (float)rgb.g / sum;
    float b_ratio = (float)rgb.b / sum;

    if (rgb.r < 30 && rgb.g < 30 && rgb.b < 30)
        return COLOR_BLACK;
    if (rgb.r > 200 && rgb.g > 200 && rgb.b > 200)
        return COLOR_WHITE;
    if (r_ratio > 0.4 && g_ratio > 0.4)
        return COLOR_YELLOW;
    if (r_ratio > g_ratio && r_ratio > b_ratio)
        return COLOR_RED;
    if (g_ratio > r_ratio && g_ratio > b_ratio)
        return COLOR_GREEN;
    if (b_ratio > r_ratio && b_ratio > g_ratio)
        return COLOR_BLUE;

    return COLOR_UNKNOWN;
}