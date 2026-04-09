#include "button.h"
#include "esp_timer.h"
#include <string.h>

typedef struct
{
    int64_t last_time;
    int64_t hold_start;
    uint8_t last_state;
    uint8_t is_held;
} ButtonState;

static ButtonState btn_reset = {0, 0, 1, 0};
static ButtonState btn_swap_mode = {0, 0, 1, 0};
static ButtonState btn_stop = {0, 0, 1, 0};

void Button_Init(void)
{
    gpio_config_t io = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};

    gpio_num_t pins[] = {
        BUTTON_RESET,
        BUTTON_SWAP_MODE,
        BUTTON_STOP};

    for (int i = 0; i < BUTTON_NUM; i++)
    {
        io.pin_bit_mask = 1ULL << pins[i];
        gpio_config(&io);
    }
}

Button_State Button_Pressing(void)
{
    struct
    {
        gpio_num_t pin;
        ButtonState *state;
        Button_State press_evt;
        Button_State hold_evt;
    } buttons[] = {
        {BUTTON_RESET, &btn_reset, BUTTON_RESET_PRESS, BUTTON_RESET_HOLD},
        {BUTTON_SWAP_MODE, &btn_swap_mode, BUTTON_SWAP_MODE_PRESS, BUTTON_SWAP_MODE_HOLD},
        {BUTTON_STOP, &btn_stop, BUTTON_STOP_PRESS, BUTTON_STOP_HOLD}};

    uint64_t now = esp_timer_get_time() / 1000;

    for (int i = 0; i < BUTTON_NUM; i++)
    {
        uint8_t state = gpio_get_level(buttons[i].pin);
        ButtonState *btn = buttons[i].state;

        if (state != btn->last_state)
        {
            if (now - btn->last_time > DEBOUNCE_TIME)
            {

                btn->last_time = now;
                btn->last_state = state;

                if (state == 0)
                {
                    btn->hold_start = now;
                    btn->is_held = 0;
                    return buttons[i].pin;
                }
                else
                {
                    if (!btn->is_held)
                    {
                        return buttons[i].press_evt;
                    }
                    btn->is_held = 0;
                }
            }
        }
        else if (state == 0)
        {
            if (buttons[i].hold_evt == 0)
                continue;
            if (!btn->is_held && (now - btn->hold_start >= HOLD_TIME))
            {
                btn->is_held = 1;
                btn->last_time = now;
                return buttons[i].hold_evt;
            }
            if (btn->is_held && (now - btn->last_time >= REPEAT_INTERVAL))
            {
                btn->last_time = now;
                return buttons[i].hold_evt;
            }
        }
    }

    return BUTTON_NONE;
}
