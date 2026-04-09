#ifndef BUTTON_H
#define BUTTON_H

#include "driver/gpio.h"
#include <stdint.h>

// Thời gian tính theo ms
#define DEBOUNCE_TIME     20
#define HOLD_TIME         1000
#define REPEAT_INTERVAL   50
#define BUTTON_NUM    3

// Khai báo GPIO nút bấm
#define BUTTON_RESET GPIO_NUM_26
#define BUTTON_SWAP_MODE GPIO_NUM_14
#define BUTTON_STOP GPIO_NUM_27

typedef enum{
	BUTTON_NONE = 0,
	BUTTON_RESET_HOLD,
	BUTTON_RESET_PRESS,
	BUTTON_SWAP_MODE_HOLD,
    BUTTON_SWAP_MODE_PRESS,
	BUTTON_STOP_PRESS,
    BUTTON_STOP_HOLD
}Button_State;

void Button_Init(void);
Button_State Button_Pressing(void);

#endif
