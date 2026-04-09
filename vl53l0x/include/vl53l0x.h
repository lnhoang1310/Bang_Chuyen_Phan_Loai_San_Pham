#ifndef VL53L0X_H
#define VL53L0X_H

#include "i2c_soft.h"

#define VL53L0X_OUT_OF_RANGE (0xFFFF)
#define REG_IDENTIFICATION_MODEL_ID (0xC0)
#define REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV (0x89)
#define REG_MSRC_CONFIG_CONTROL (0x60)
#define REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT (0x44)
#define REG_SYSTEM_SEQUENCE_CONFIG (0x01)
#define REG_DYNAMIC_SPAD_REF_EN_START_OFFSET (0x4F)
#define REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD (0x4E)
#define REG_GLOBAL_CONFIG_REF_EN_START_SELECT (0xB6)
#define REG_SYSTEM_INTERRUPT_CONFIG_GPIO (0x0A)
#define REG_GPIO_HV_MUX_ACTIVE_HIGH (0x84)
#define REG_SYSTEM_INTERRUPT_CLEAR (0x0B)
#define REG_RESULT_INTERRUPT_STATUS (0x13)
#define REG_SYSRANGE_START (0x00)
#define REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0 (0xB0)
#define REG_RESULT_RANGE_STATUS (0x14)
#define REG_SLAVE_DEVICE_ADDRESS (0x8A)

#define RANGE_SEQUENCE_STEP_TCC (0x10)  /* Target CentreCheck */
#define RANGE_SEQUENCE_STEP_MSRC (0x04) /* Minimum Signal Rate Check */
#define RANGE_SEQUENCE_STEP_DSS (0x28)  /* Dynamic SPAD selection */
#define RANGE_SEQUENCE_STEP_PRE_RANGE (0x40)
#define RANGE_SEQUENCE_STEP_FINAL_RANGE (0x80)

#define VL53L0X_EXPECTED_DEVICE_ID (0xEE)
#define VL53L0X_DEFAULT_ADDRESS (0x29)
#define VL53L0X_ADDRESS_START (uint8_t)(0x30)

#define SPAD_TYPE_APERTURE (0x01)
#define SPAD_START_SELECT (0xB4)
#define SPAD_MAX_COUNT (44)
#define SPAD_MAP_ROW_COUNT (6)
#define SPAD_ROW_SIZE (8)
#define SPAD_APERTURE_START_INDEX (12)

#define NUMS_SENSOR (8)

typedef enum
{
    CALIBRATION_TYPE_VHV,
    CALIBRATION_TYPE_PHASE
} calibration_type_t;

typedef enum
{
    RIGHT,
    HALF_RIGHT,
    LEFT,
    HALF_LEFT
} VL53L0X_Position;

typedef enum
{
    VL53L0X_OK,
    VL53L0X_ERROR
} VL53L0X_State;

typedef struct
{
    i2c_soft_typedef *i2c;
    uint8_t address;
    gpio_num_t xshut_pin;
    VL53L0X_Position position;
    uint16_t distance_mm;
    float distance_m;
    float distance_cm;
    int8_t offset;
    VL53L0X_State state;
} VL53L0X_TypeDef;

extern VL53L0X_TypeDef *list_sensor[NUMS_SENSOR];

bool vl53l0x_init(VL53L0X_TypeDef *sensor, i2c_soft_typedef *_i2c, gpio_num_t _xshut_pin, VL53L0X_Position _position, int8_t _offset);
bool vl53l0x_read_range_single(VL53L0X_TypeDef *sensor);
bool vl53l0x_read_all_sensor(void);

#endif