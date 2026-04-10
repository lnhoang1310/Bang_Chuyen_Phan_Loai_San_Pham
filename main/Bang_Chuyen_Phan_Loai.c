#include <stdio.h>
#include "button.h"
#include "vl53l0x.h"
#include "tcs3200.h"
#include "lcd.h"
#include "i2c_soft.h"
#include "servo.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Define Parameter*/
#define IR1_SENSOR GPIO_NUM_34
#define IR2_SENSOR GPIO_NUM_35
#define IR3_SENSOR GPIO_NUM_36
#define RELAY_MOTOR GPIO_NUM_4
#define XSHUT_PIN GPIO_NUM_5
#define DISTANCE_DEFAULT 10
#define Motor_On() gpio_set_level(RELAY_MOTOR, 1)
#define Motor_Off() gpio_set_level(RELAY_MOTOR, 0)
#define Reset_System() esp_restart()

/* Define Struct - Enum*/
typedef enum
{
    MODE_HEIGHT,
    MODE_COLOR
} Classification_Mode;

typedef struct
{
    uint16_t product_count_type_1;
    uint16_t product_count_type_2;
    uint16_t product_count_type_3;
} Production_Count;

typedef enum
{
    PRODUCT_UNKNOWN = 0,
    PRODUCT_TYPE_1,
    PRODUCT_TYPE_2,
    PRODUCT_TYPE_3
} Product_Type;

typedef enum
{
    PROCESS_IDLE,
    PROCESS_DETECTING,
    PROCESS_CLASSIFY,
    PROCESS_WAIT_SERVO
} Process_State;

/* Global Struct Variable*/
lcd_typedef lcd;
Classification_Mode current_mode = MODE_HEIGHT;
VL53L0X_TypeDef distance_sensor;
tcs3200_color_t color;
i2c_soft_typedef i2c;
Servo_Typedef servo1, servo2;
Button_State button = BUTTON_NONE;
Production_Count production_count_type_hight = {0, 0, 0};
Production_Count production_count_type_color = {0, 0, 0};
Product_Type current_product_type = PRODUCT_UNKNOWN;
Process_State process_state = PROCESS_IDLE;
/*Global Variable*/
volatile bool flag_product_detected = false;
volatile bool flag_servo1_activated = false;
volatile bool flag_servo2_activated = false;

/* ISR */
void IRAM_ATTR IR_Sensor_ISR(void *arg)
{
    int sensor_num = (int)arg;
    if (sensor_num == IR1_SENSOR)
        flag_product_detected = true;
    else if (sensor_num == IR2_SENSOR)
        flag_servo1_activated = true;
    else if (sensor_num == IR3_SENSOR)
        flag_servo2_activated = true;
}

void display()
{
    static float product_height = 0;
    static tcs3200_color_t product_color = COLOR_UNKNOWN;
    char buffer[30];
    lcd_setcursor(&lcd, 0, 0);
    if (current_mode == MODE_HEIGHT)
    {
        float height = DISTANCE_DEFAULT - distance_sensor.distance_cm;
        if (height > 2)
            product_height = height;
        if (flag_product_detected && product_height > 2)
        {
            sprintf(buffer, "HEIGHT: %3.1f cm     ", product_height);
            lcd_sendstring(&lcd, buffer);
        }
        else
        {
            lcd_sendstring(&lcd, "HEIGHT: -- mm   ");
            product_height = 0;
        }
        lcd_setcursor(&lcd, 1, 0);
        sprintf(buffer, "L1:%d L2:%d L3:%d", production_count_type_hight.product_count_type_1, production_count_type_hight.product_count_type_2, production_count_type_hight.product_count_type_3);
    }
    else if (current_mode == MODE_COLOR)
    {
        if (color != COLOR_UNKNOWN)
            product_color = color;
        if (flag_product_detected && product_color != COLOR_UNKNOWN)
        {
            sprintf(buffer, "COLOR: %s       ", (color == COLOR_RED) ? "RED" : (color == COLOR_GREEN) ? "GREEN"
                                                                           : (color == COLOR_BLUE)    ? "BLUE"
                                                                                                      : "UNKNOWN");
            lcd_sendstring(&lcd, buffer);
        }
        else
        {
            lcd_sendstring(&lcd, "COLOR: --       ");
            product_color = COLOR_UNKNOWN;
        }
        lcd_setcursor(&lcd, 1, 0);
        sprintf(buffer, "L1:%d L2:%d L3:%d", production_count_type_color.product_count_type_1, production_count_type_color.product_count_type_2, production_count_type_color.product_count_type_3);
    }
}

Product_Type classify_product(void)
{
    if (current_mode == MODE_HEIGHT)
    {
        float height = DISTANCE_DEFAULT - distance_sensor.distance_cm;
        if (height > 2 && height < 5)
        {
            production_count_type_hight.product_count_type_1++;
            return PRODUCT_TYPE_1;
        }
        else if (height >= 5 && height < 8)
        {
            production_count_type_hight.product_count_type_2++;
            return PRODUCT_TYPE_2;
        }
        else if (height >= 8)
        {
            production_count_type_hight.product_count_type_3++;
            return PRODUCT_TYPE_3;
        }
    }
    else if (current_mode == MODE_COLOR)
    {
        if (color == COLOR_RED)
        {
            production_count_type_color.product_count_type_1++;
            return PRODUCT_TYPE_1;
        }
        else if (color == COLOR_GREEN)
        {
            production_count_type_color.product_count_type_2++;
            return PRODUCT_TYPE_2;
        }
        else if (color == COLOR_BLUE)
        {
            production_count_type_color.product_count_type_3++;
            return PRODUCT_TYPE_3;
        }
    }
    return PRODUCT_UNKNOWN;
}

void process(void)
{
    switch (process_state)
    {
    case PROCESS_IDLE:
        if (flag_product_detected)
        {
            Servo_SetAngle(&servo1, 0);
            Servo_SetAngle(&servo2, 0);
            color = COLOR_UNKNOWN;
            distance_sensor.distance_cm = DISTANCE_DEFAULT;
            process_state = PROCESS_DETECTING;
        }
        break;
    case PROCESS_DETECTING:
        if (color == COLOR_UNKNOWN)
            color = tcs3200_get_color();
        if (distance_sensor.distance_cm > DISTANCE_DEFAULT - 2)
            vl53l0x_read_range_single(&distance_sensor);
        if (color != COLOR_UNKNOWN && distance_sensor.distance_cm <= DISTANCE_DEFAULT - 2)
            process_state = PROCESS_CLASSIFY;
        break;
    case PROCESS_CLASSIFY:
        current_product_type = classify_product();
        flag_product_detected = false;
        process_state = PROCESS_WAIT_SERVO;
        break;
    case PROCESS_WAIT_SERVO:
        if (current_product_type == PRODUCT_TYPE_1 && flag_servo1_activated)
        {
            Servo_SetAngle(&servo1, 90);
            flag_servo1_activated = false;
            process_state = PROCESS_IDLE;
        }
        else if (current_product_type == PRODUCT_TYPE_2 && flag_servo2_activated)
        {
            Servo_SetAngle(&servo2, 90);
            flag_servo2_activated = false;
            process_state = PROCESS_IDLE;
        }
        else if (current_product_type == PRODUCT_TYPE_3)
            process_state = PROCESS_IDLE;
        break;
    }
}

void app_main(void)
{
    // Initialize peripherals
    lcd_init(&lcd, I2C_NUM_0, LCD_ADDRESS, GPIO_NUM_21, GPIO_NUM_22);
    i2c_soft_init(&i2c, GPIO_NUM_21, GPIO_NUM_22);
    vl53l0x_init(&distance_sensor, &i2c, XSHUT_PIN, RIGHT, 0);
    tcs3200_init();
    Button_Init();
    Servo_Init(&servo1, SERVO1_TIMER, SERVO1_CHANNEL, SERVO1_GPIO);
    Servo_Init(&servo2, SERVO2_TIMER, SERVO2_CHANNEL, SERVO2_GPIO);

    // Calibrate sensors
    tcs3200_calibrate_white();
    tcs3200_calibrate_black();

    // Set up IR sensor interrupts
    gpio_set_intr_type(IR1_SENSOR, GPIO_INTR_POSEDGE);
    gpio_set_intr_type(IR2_SENSOR, GPIO_INTR_POSEDGE);
    gpio_set_intr_type(IR3_SENSOR, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(IR1_SENSOR, IR_Sensor_ISR, (void *)IR1_SENSOR);
    gpio_isr_handler_add(IR2_SENSOR, IR_Sensor_ISR, (void *)IR2_SENSOR);
    gpio_isr_handler_add(IR3_SENSOR, IR_Sensor_ISR, (void *)IR3_SENSOR);

    while (1)
    {
        button = Button_Pressing();
        switch (button)
        {
        case BUTTON_RESET_PRESS:
            Reset_System();
            break;
        case BUTTON_SWAP_MODE_PRESS:
            current_mode = (current_mode == MODE_HEIGHT) ? MODE_COLOR : MODE_HEIGHT;
            break;
        default:
            break;
        }
        display();
        process();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
