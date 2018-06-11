#include <spi/spi.h>
#include <pex/pex.h>

// For writing to the communication register
// (when starting a communication operation with the ADC)
// to specify whether the following operation will be read or write
#define COMM_BYTE_WRITE       0b00000000
#define COMM_BYTE_READ_CONT   0b01000100  // CREAD is set
#define COMM_BYTE_READ_SINGLE 0b01000000  // single conversion
// bit[5:3] is register address
// bit[2] is CREAD. Set to 1 to enable continuous read

// Register addresses (p.20)
#define STATUS_ADDR     0x00
#define MODE_ADDR       0x01
#define CONFIG_ADDR     0x02
#define DATA_ADDR       0x03
#define ID_ADDR         0x04
#define GPOCON_ADDR     0x05
#define OFFSET_ADDR     0x06
#define FULL_SCALE_ADDR 0x07

#define CONFIG_DEFAULT  0x040008

// Current default mode settings are alright.
// #define MODE_DEFAULT    0x00

// GPIOB pins on the PAY_SENSOR board's port expander
#define ADC_CS 0
#define ITF_CS 1

// For ADC read conversion (p.31)
#define N 24      // number of bits read
#define V_REF 2.5 // reference voltage

// ADC channels (see KiCad schematic)
#define TEMD_ADC_CHANNEL  5
#define SFH_ADC_CHANNEL   6
#define ADPD_ADC_CHANNEL  7

// Mode select bits for continuous conversion or calibration (p. 21-23, 37)
#define CONT_CONV             0x0
#define INT_ZERO_SCALE_CALIB  0x4
#define INT_FULL_SCALE_CALIB  0x5
#define SYS_ZERO_SCALE_CALIB  0x6
#define SYS_FULL_SCALE_CALIB  0x7


void init_adc(void);
void adc_pex_hard_rst(void);
uint32_t read_ADC_register(uint8_t register_addr);
void write_ADC_register(uint8_t register_addr, uint32_t data);

void select_ADC_channel(uint8_t channel_num);
uint32_t read_ADC_channel(uint8_t channel_num);
double convert_ADC_reading(uint32_t ADC_reading, uint8_t pga_gain);

uint8_t num_register_bytes(uint8_t register_addr);
void set_PGA(uint8_t gain);
uint8_t convert_gain_bits(uint8_t gain);
