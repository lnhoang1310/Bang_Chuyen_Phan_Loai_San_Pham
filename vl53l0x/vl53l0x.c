#include "vl53l0x.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

VL53L0X_TypeDef *list_sensor[NUMS_SENSOR] = {NULL};
static uint8_t stop_variable = 0;
static uint8_t sensor_index = 0;

static bool device_is_booted(i2c_soft_typedef *i2c)
{
    uint8_t device_id = 0;
    if (!i2c_soft_read_addr8_data8(i2c, VL53L0X_DEFAULT_ADDRESS, REG_IDENTIFICATION_MODEL_ID, &device_id))
    {
        return false;
    }
    return device_id == VL53L0X_EXPECTED_DEVICE_ID;
}

static bool data_init(VL53L0X_TypeDef *sensor)
{
    bool success = false;
    uint8_t vhv_config_scl_sda = 0;
    if (!i2c_soft_read_addr8_data8(sensor->i2c, sensor->address, REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV, &vhv_config_scl_sda))
    {
        return false;
    }
    vhv_config_scl_sda |= 0x01;
    if (!i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV, vhv_config_scl_sda))
    {
        return false;
    }

    success = i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x88, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x80, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x00, 0x00);
    success &= i2c_soft_read_addr8_data8(sensor->i2c, sensor->address, 0x91, &stop_variable);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x00, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x80, 0x00);
    return success;
}

static bool read_strobe(VL53L0X_TypeDef *sensor)
{
    bool success = false;
    uint8_t strobe = 0;
    if (!i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x83, 0x00))
    {
        return false;
    }
    do
    {
        success = i2c_soft_read_addr8_data8(sensor->i2c, sensor->address, 0x83, &strobe);
    } while (success && (strobe == 0));
    if (!success)
    {
        return false;
    }
    if (!i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x83, 0x01))
    {
        return false;
    }
    return true;
}

static bool get_spad_info_from_nvm(VL53L0X_TypeDef *sensor, uint8_t *spad_count, uint8_t *spad_type, uint8_t good_spad_map[6])
{
    bool success = false;
    uint8_t tmp_data8 = 0;
    uint32_t tmp_data32 = 0;

    success = i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x80, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x00, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x06);
    success &= i2c_soft_read_addr8_data8(sensor->i2c, sensor->address, 0x83, &tmp_data8);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x83, tmp_data8 | 0x04);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x07);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x81, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x80, 0x01);
    if (!success)
    {
        return false;
    }

    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x94, 0x6b);
    if (!success || !read_strobe(sensor) || !i2c_soft_read_addr8_data32(sensor->i2c, sensor->address, 0x90, &tmp_data32))
    {
        return false;
    }
    *spad_count = (tmp_data32 >> 8) & 0x7f;
    *spad_type = (tmp_data32 >> 15) & 0x01;

    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x81, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x06);
    success &= i2c_soft_read_addr8_data8(sensor->i2c, sensor->address, 0x83, &tmp_data8);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x83, tmp_data8 & 0xfb);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x00, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x80, 0x00);

    if (!i2c_soft_read_addr8_bytes(sensor->i2c, sensor->address, REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0, good_spad_map, 6))
    {
        return false;
    }
    return success;
}

static bool set_spads_from_nvm(VL53L0X_TypeDef *sensor)
{
    uint8_t spad_map[SPAD_MAP_ROW_COUNT] = {0};
    uint8_t good_spad_map[SPAD_MAP_ROW_COUNT] = {0};
    uint8_t spads_enabled_count = 0;
    uint8_t spads_to_enable_count = 0;
    uint8_t spad_type = 0;
    volatile uint32_t total_val = 0;

    if (!get_spad_info_from_nvm(sensor, &spads_to_enable_count, &spad_type, good_spad_map))
    {
        return false;
    }

    for (int i = 0; i < 6; i++)
    {
        total_val += good_spad_map[i];
    }

    bool success = i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, REG_DYNAMIC_SPAD_REF_EN_START_OFFSET, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD, 0x2C);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, REG_GLOBAL_CONFIG_REF_EN_START_SELECT, SPAD_START_SELECT);
    if (!success)
    {
        return false;
    }

    uint8_t offset = (spad_type == SPAD_TYPE_APERTURE) ? SPAD_APERTURE_START_INDEX : 0;

    for (int row = 0; row < SPAD_MAP_ROW_COUNT; row++)
    {
        for (int column = 0; column < SPAD_ROW_SIZE; column++)
        {
            int index = (row * SPAD_ROW_SIZE) + column;
            if (index >= SPAD_MAX_COUNT)
            {
                return false;
            }
            if (spads_enabled_count == spads_to_enable_count)
            {
                break;
            }
            if (index < offset)
            {
                continue;
            }
            if ((good_spad_map[row] >> column) & 0x1)
            {
                spad_map[row] |= (1 << column);
                spads_enabled_count++;
            }
        }
        if (spads_enabled_count == spads_to_enable_count)
        {
            break;
        }
    }

    if (spads_enabled_count != spads_to_enable_count)
    {
        return false;
    }

    if (!i2c_soft_write_addr8_bytes(sensor->i2c, sensor->address, REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0, spad_map, SPAD_MAP_ROW_COUNT))
    {
        return false;
    }

    return true;
}

static bool load_default_tuning_settings(VL53L0X_TypeDef *sensor)
{
    bool success = i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x00, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x09, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x10, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x11, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x24, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x25, 0xFF);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x75, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x4E, 0x2C);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x48, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x30, 0x20);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x30, 0x09);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x54, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x31, 0x04);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x32, 0x03);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x40, 0x83);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x46, 0x25);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x60, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x27, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x50, 0x06);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x51, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x52, 0x96);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x56, 0x08);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x57, 0x30);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x61, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x62, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x64, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x65, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x66, 0xA0);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x22, 0x32);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x47, 0x14);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x49, 0xFF);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x4A, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x7A, 0x0A);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x7B, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x78, 0x21);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x23, 0x34);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x42, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x44, 0xFF);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x45, 0x26);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x46, 0x05);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x40, 0x40);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x0E, 0x06);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x20, 0x1A);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x43, 0x40);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x34, 0x03);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x35, 0x44);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x31, 0x04);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x4B, 0x09);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x4C, 0x05);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x4D, 0x04);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x44, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x45, 0x20);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x47, 0x08);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x48, 0x28);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x67, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x70, 0x04);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x71, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x72, 0xFE);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x76, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x77, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x0D, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x80, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x01, 0xF8);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x8E, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x00, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x80, 0x00);
    return success;
}

static bool configure_interrupt(VL53L0X_TypeDef *sensor)
{
    if (!i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, REG_SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04))
    {
        return false;
    }

    uint8_t gpio_hv_mux_active_high = 0;
    if (!i2c_soft_read_addr8_data8(sensor->i2c, sensor->address, REG_GPIO_HV_MUX_ACTIVE_HIGH, &gpio_hv_mux_active_high))
    {
        return false;
    }
    gpio_hv_mux_active_high &= ~0x10;
    if (!i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, REG_GPIO_HV_MUX_ACTIVE_HIGH, gpio_hv_mux_active_high))
    {
        return false;
    }

    if (!i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, REG_SYSTEM_INTERRUPT_CLEAR, 0x01))
    {
        return false;
    }
    return true;
}

static bool set_sequence_steps_enabled(VL53L0X_TypeDef *sensor, uint8_t sequence_step)
{
    return i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, REG_SYSTEM_SEQUENCE_CONFIG, sequence_step);
}

static bool static_init(VL53L0X_TypeDef *sensor)
{
    if (!set_spads_from_nvm(sensor))
    {
        return false;
    }
    if (!load_default_tuning_settings(sensor))
    {
        return false;
    }
    if (!configure_interrupt(sensor))
    {
        return false;
    }
    if (!set_sequence_steps_enabled(sensor, RANGE_SEQUENCE_STEP_DSS +
                                                RANGE_SEQUENCE_STEP_PRE_RANGE +
                                                RANGE_SEQUENCE_STEP_FINAL_RANGE))
    {
        return false;
    }
    return true;
}

static bool perform_single_ref_calibration(VL53L0X_TypeDef *sensor, calibration_type_t calib_type)
{
    uint8_t sysrange_start = 0;
    uint8_t sequence_config = 0;
    switch (calib_type)
    {
    case CALIBRATION_TYPE_VHV:
        sequence_config = 0x01;
        sysrange_start = 0x01 | 0x40;
        break;
    case CALIBRATION_TYPE_PHASE:
        sequence_config = 0x02;
        sysrange_start = 0x01 | 0x00;
        break;
    }
    if (!i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, REG_SYSTEM_SEQUENCE_CONFIG, sequence_config))
    {
        return false;
    }
    if (!i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, REG_SYSRANGE_START, sysrange_start))
    {
        return false;
    }
    uint8_t interrupt_status = 0;
    bool success = false;
    do
    {
        success = i2c_soft_read_addr8_data8(sensor->i2c, sensor->address, REG_RESULT_INTERRUPT_STATUS, &interrupt_status);
    } while (success && ((interrupt_status & 0x07) == 0));
    if (!success)
    {
        return false;
    }
    if (!i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, REG_SYSTEM_INTERRUPT_CLEAR, 0x01))
    {
        return false;
    }
    if (!i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, REG_SYSRANGE_START, 0x00))
    {
        return false;
    }
    return true;
}

static bool perform_ref_calibration(VL53L0X_TypeDef *sensor)
{
    if (!perform_single_ref_calibration(sensor, CALIBRATION_TYPE_VHV))
    {
        return false;
    }
    if (!perform_single_ref_calibration(sensor, CALIBRATION_TYPE_PHASE))
    {
        return false;
    }
    if (!set_sequence_steps_enabled(sensor, RANGE_SEQUENCE_STEP_DSS + RANGE_SEQUENCE_STEP_PRE_RANGE + RANGE_SEQUENCE_STEP_FINAL_RANGE))
    {
        return false;
    }
    return true;
}

static bool configure_address(i2c_soft_typedef *i2c, uint8_t addr)
{
    return i2c_soft_write_addr8_data8(i2c, VL53L0X_DEFAULT_ADDRESS, REG_SLAVE_DEVICE_ADDRESS, addr & 0x7F);
}

static void set_hardware_standby(VL53L0X_TypeDef *sensor, bool enable)
{
    gpio_set_level(sensor->xshut_pin, enable ? 0 : 1);
}

static void configure_gpio(VL53L0X_TypeDef *sensor)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << sensor->xshut_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);
    gpio_set_level(sensor->xshut_pin, 0);
}

static bool init_address(VL53L0X_TypeDef *sensor)
{
    set_hardware_standby(sensor, false);
    vTaskDelay(pdMS_TO_TICKS(1));
    if (!device_is_booted(sensor->i2c))
    {
        return false;
    }
    if (!configure_address(sensor->i2c, sensor->address))
    {
        return false;
    }
    return true;
}

static bool init_addresses(VL53L0X_TypeDef *sensor)
{
    configure_gpio(sensor);
    if (!init_address(sensor))
    {
        return false;
    }
    return true;
}

static bool init_config(VL53L0X_TypeDef *sensor)
{
    if (!data_init(sensor))
    {
        return false;
    }
    if (!static_init(sensor))
    {
        return false;
    }
    if (!perform_ref_calibration(sensor))
    {
        return false;
    }
    return true;
}

bool vl53l0x_init(VL53L0X_TypeDef *sensor, i2c_soft_typedef *_i2c, gpio_num_t _xshut_pin, VL53L0X_Position _position, int8_t _offset)
{
    sensor->i2c = _i2c;
    sensor->xshut_pin = _xshut_pin;
    sensor->position = _position;
    sensor->address = VL53L0X_ADDRESS_START | sensor_index;
    sensor->offset = _offset;
    sensor->distance_mm = 0;
    sensor->distance_m = 0;
    sensor->distance_cm = 0;
    list_sensor[sensor_index++] = sensor;

    if (!init_addresses(sensor))
    {
        sensor->state = VL53L0X_ERROR;
        return false;
    }
    if (!init_config(sensor))
    {
        sensor->state = VL53L0X_ERROR;
        return false;
    }
    sensor->state = VL53L0X_OK;
    return true;
}

bool vl53l0x_read_range_single(VL53L0X_TypeDef *sensor)
{
    bool success = i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x80, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x00, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x91, stop_variable);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x00, 0x01);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0xFF, 0x00);
    success &= i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, 0x80, 0x00);
    if (!success)
    {
        return false;
    }

    if (!i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, REG_SYSRANGE_START, 0x01))
    {
        return false;
    }

    uint8_t sysrange_start = 0;
    do
    {
        success = i2c_soft_read_addr8_data8(sensor->i2c, sensor->address, REG_SYSRANGE_START, &sysrange_start);
    } while (success && (sysrange_start & 0x01));
    if (!success)
    {
        return false;
    }

    uint8_t interrupt_status = 0;
    do
    {
        success = i2c_soft_read_addr8_data8(sensor->i2c, sensor->address, REG_RESULT_INTERRUPT_STATUS, &interrupt_status);
    } while (success && ((interrupt_status & 0x07) == 0));
    if (!success)
    {
        return false;
    }

    if (!i2c_soft_read_addr8_data16(sensor->i2c, sensor->address, REG_RESULT_RANGE_STATUS + 10, &sensor->distance_mm))
    {
        return false;
    }

    if (!i2c_soft_write_addr8_data8(sensor->i2c, sensor->address, REG_SYSTEM_INTERRUPT_CLEAR, 0x01))
    {
        return false;
    }

    if (sensor->distance_mm == 8190 || sensor->distance_mm == 8191)
    {
        sensor->distance_mm = VL53L0X_OUT_OF_RANGE;
    }
    sensor->distance_m = (float)(sensor->distance_mm + sensor->offset) / 1000.0f;
    sensor->distance_cm = sensor->distance_m * 100;
    return true;
}

bool vl53l0x_read_all_sensor(void)
{
    bool flag = true;
    for (uint8_t i = 0; i < sensor_index; i++)
    {
        if (!vl53l0x_read_range_single(list_sensor[i]))
        {
            list_sensor[i]->state = VL53L0X_ERROR;
            flag = false;
        }
    }
    return flag;
}
