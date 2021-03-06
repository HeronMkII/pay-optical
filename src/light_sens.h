#ifndef LIGHT_SENS_H
#define LIGHT_SENS_H

#include <stdint.h>
#include <i2c/i2c.h>

/*
For more information, check out pg 13 of the datasheet
"Devices with primary address 0x29 have a secondary address
of 0x28 that can be used for read only registers for a quick
single-block I2C transaction"
*/

#define LSENSE_ADDRESS              0x29
#define LSENSE_ADDRESS_READ_ONLY    0x28

/* REGISTER ADDRESSES (7-bit) */
/* WRITE ONLY */
#define LSENSE_COMMAND_BYTE     0b10100000
/* READ/WRITE ACCESS */
#define LSENSE_ENABLE           0x00
#define LSENSE_CONTROL          0x01
#define LSENSE_AILTL            0x04
#define LSENSE_AILTH            0x05
#define LSENSE_AIHTL            0x06
#define LSENSE_AIHTH            0x07
#define LSENSE_NPAILTL          0x08
#define LSENSE_NPAILTH          0x09
#define LSENSE_NPAIHTL          0x0A
#define LSENSE_NPAIHTH          0x0B
#define LSENSE_PERSIST          0x0C
/* READ ACCESS */
#define LSENSE_PID              0x11
#define LSENSE_ID               0x12
#define LSENSE_STATUS           0x13
#define LSENSE_C0DATAL          0x14
#define LSENSE_C0DATAH          0x15
#define LSENSE_C1DATAL          0x16
#define LSENSE_C1DATAH          0x17

/* DEFAULT REGISTER VALUES */
// Enables the device and powers on the oscillator
#define LSENSE_DEF_ENABLE       0b00000011
// No reset, low gain, 200ms integration time
#define LSENSE_DEF_CONTROL      0b00000001

/* QUALITY OF LIFE DEFINES */ 
#define LSENSE_AGAIN_MASK   0xCF
#define LSENSE_ATIME_MASK   0xF8

typedef enum {
    LS_DISABLED = 0,
    LS_ENABLED = 1
} light_sensor_state_t;

typedef enum {
    LS_LOW_GAIN =   0b00,
    LS_MED_GAIN =   0b01,
    LS_HIGH_GAIN =  0b10,
    LS_MAX_GAIN =   0b11
} light_sensor_again_t;

typedef enum {
    LS_100ms =  0b000,
    LS_200ms =  0b001,
    LS_300ms =  0b010,
    LS_400ms =  0b011,
    LS_500ms =  0b100,
    LS_600ms =  0b101
} light_sensor_atime_t;

typedef struct {
    light_sensor_again_t gain;
    light_sensor_atime_t time;
} light_sensor_setting_t;

// TSL2591 device
typedef struct {
    // 0 == disabled, 1 == enabled
    light_sensor_state_t state;

    light_sensor_again_t gain;
    light_sensor_atime_t time;

    uint16_t last_ch0_reading;
    uint16_t last_ch1_reading;
} light_sensor_t;

/* FUNCTION PROTOTYPES */
void write_light_sense_register(uint8_t addr, uint8_t data);
uint8_t read_light_sense_register(uint8_t addr);
void get_light_sense_read_only(uint8_t* data);

void init_light_sensor(light_sensor_t* light_sens);
void sleep_light_sensor(light_sensor_t* light_sens);
void wake_light_sensor(light_sensor_t* light_sens);
void get_light_sensor_readings(light_sensor_t* light_sens);
void set_light_sensor_again(light_sensor_t* light_sens);
void set_light_sensor_atime(light_sensor_t* light_sens);

#endif