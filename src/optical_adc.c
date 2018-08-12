/*
DESCRIPTION: AD7194 ADC functions
AUTHORS: Bruno Almeida, Dylan Vogel

This is referred to as the "optical ADC", which is different from the primary
ADC library in lib-common.

- single conversion mode

TODO:
* Add a function to automatically scale gain
* Add calibration function
* Use 4 GPIO pins for mux A0, A1, A2, EN
* Switch GPIO outputs to mux when appropriate, create function
*/

#include "optical_adc.h"

void opt_adc_init(void){
    // Initialize ports and registers needed for ADC usage

    init_cs(CS_PIN, &CS_DDR);
    set_cs_high(CS_PIN, &CS_PORT);

    // opt_adc_write_reg(CONFIG_ADDR, CONFIG_DEFAULT);
    // "Continuous conversion is the default power-up mode." (p. 32)

    // TODO - start in power down mode

    // GPOCON register - enable 4 GPIO outputs
    // _EN_ = 1, A2/A1/A0 = 0
    opt_adc_write_reg(GPOCON_ADDR, 0b00111000);

    opt_adc_init_config();
    opt_adc_init_mode();
}


// Initializes configuration register
void opt_adc_init_config(void) {
    uint32_t config = opt_adc_read_reg(CONFIG_ADDR);

    // Enable pseudo-differential output
    // Enable unipolar (positive voltage) mode
    // Set gain to 0b000 (PGA = 1)
    config = config | CONFIG_PSEUDO;
    config = config | CONFIG_UNIPOLAR;
    config = config & 0xFFFFF8;

    opt_adc_write_reg(CONFIG_ADDR, config);
}


// Initializes mode register
void opt_adc_init_mode(void) {
    uint32_t mode = opt_adc_read_reg(MODE_ADDR);

    // Clear first 3 bits and set operating mode to power down mode
    mode = mode & 0x1FFFFF;
    mode = mode | MODE_POWER_DOWN;

    opt_adc_write_reg(MODE_ADDR, mode);
}


uint32_t opt_adc_read_reg(uint8_t register_addr) {
    // Read the current state of the specified ADC register.

    register_addr = register_addr & 0b111;

    // Start communication
    set_cs_low(CS_PIN, &CS_PORT);

    // Begin communication with a write to the communications register
    // Request a read from the specified register
    send_spi(COMM_READ | (register_addr << 3));

    // Read the required number of bytes based on register
    uint32_t data = 0;
    for (uint8_t i = 0; i < opt_adc_num_reg_bytes(register_addr); i++) {
        data = data << 8;
        data = data | send_spi(0x00);
    }

    // Stop communication
    set_cs_high(CS_PIN, &CS_PORT);

    return data;
}


void opt_adc_write_reg(uint8_t register_addr, uint32_t data) {
    // Writes a new state to the specified ADC register.

    register_addr = register_addr & 0b111;

    // Start communication
    set_cs_low(CS_PIN, &CS_PORT);

    // Begin communication with a write to the communications register
    // Request a write to the specified register
    send_spi(COMM_WRITE | (register_addr << 3));

    // Write the number of bytes in the register
    for (int8_t i = opt_adc_num_reg_bytes(register_addr) - 1; i >= 0; i--) {
        send_spi( (uint8_t) (data >> (i * 8)) );
    }

    // Set CS high
    set_cs_high(CS_PIN, &CS_PORT);
}


void opt_adc_select_channel(uint8_t channel_num) {
    // Sets the configuration register's bits for the specified ADC input channel.

    // Get the 4 bits for the channel for psuedo-differential positive input (p. 26)
    uint8_t channel_bits = (channel_num - 1) << 4;

    // Read the current register status
    uint32_t config = opt_adc_read_reg(CONFIG_ADDR);

    // Mask the channel bits and write new channel
    config &= 0xffff00ff;
    config |= (channel_bits << 8);

    // Write modified config register
    opt_adc_write_reg(CONFIG_ADDR, config);
}


void opt_adc_select_pga(uint8_t gain) {
    // Sets the configuration register's bits for a specified programmable gain.
    // gain - one of 1, 8, 16, 32, 64, 128 (2 and 4 are reserved/unavailable, see p. 25)

    // Convert gain to 3 bits
    uint8_t gain_bits = opt_adc_gain_to_gain_bits(gain);

    // Read from configuration register
    uint32_t config_data = opt_adc_read_reg(CONFIG_ADDR);

    // Clear gain bits and set
    config_data &= 0xfffff8;
    config_data |= gain_bits;

    // Write to configuration register
    opt_adc_write_reg(CONFIG_ADDR, config_data);
}


// Selects the operating mode
void opt_adc_select_op_mode(uint32_t mode_bits) {
    // Read from mode register
    uint32_t mode_reg = opt_adc_read_reg(MODE_ADDR);

    // Clear gain bits and set
    mode_reg &= 0x1fffff;
    mode_reg |= mode_bits;

    // Write to configuration register
    opt_adc_write_reg(MODE_ADDR, mode_reg);
}


uint32_t opt_adc_read_channel_raw_data(uint8_t channel_num, uint8_t gain) {
    // Reads 24 bit raw data from the specified ADC channel.

    // Select the channel for conversion
    opt_adc_select_channel(channel_num);
    opt_adc_select_pga(gain);
    opt_adc_select_op_mode(MODE_SINGLE_CONV);

    // Wait until the conversion finishes, signalled by (DOUT/_RDY_/MISO) going low
    uint16_t timeout = 1023;
    while (bit_is_set(PINB, MISO_PIN) && timeout > 0) {
        timeout--;
    }

    // Read back the conversion result
    return opt_adc_read_reg(DATA_ADDR);
}


uint8_t opt_adc_num_reg_bytes(uint8_t register_addr) {
    // Returns the number of BYTES in the specified register at the given address

    switch (register_addr) {
        case STATUS_ADDR:
            return 1;
            break;
        case MODE_ADDR:
            return 3;
            break;
        case CONFIG_ADDR:
            return 3;
            break;
        case DATA_ADDR:
            return 3;
            // TODO - check byte of status information (enabled by setting DAT_STA, p. 20-21)
            break;
        case ID_ADDR:
            return 1;
            break;
        case GPOCON_ADDR:
            return 1;
            break;
        case OFFSET_ADDR:
            return 3;
            break;
        case FULL_SCALE_ADDR:
            return 3;
            break;
        default:
            return 0;
            break;
    }
}


uint8_t opt_adc_gain_to_gain_bits(uint8_t gain) {
    // Converts the numerical gain to 3 gain select bits (p.25).
    // gain - one of { 1, 8, 16, 32, 64, 128 }

    switch (gain) {
        case 1:
            return 0b000;
            break;
        case 8:
            return 0b011;
            break;
        case 16:
            return 0b100;
            break;
        case 32:
            return 0b101;
            break;
        case 64:
            return 0b110;
            break;
        case 128:
            return 0b111;
            break;
        default:
            return 0b000;
            break;
    }
}


// channel - channel to select (between 1 and 8 to represent S1-S8)
void opt_adc_enable_mux(uint8_t channel) {
    uint8_t channel_bits = channel - 1;

    // Set _EN_ (bit 3) = 0, bits 2-0 = channel
    uint8_t gpocon = opt_adc_read_reg(GPOCON_ADDR);
    gpocon = gpocon & 0xF0;
    gpocon = gpocon | channel_bits;
    opt_adc_write_reg(GPOCON_ADDR, gpocon);
}


void opt_adc_disable_mux(void) {
    // Set _EN_ (bit 3) = 1
    uint8_t gpocon = opt_adc_read_reg(GPOCON_ADDR);
    gpocon = gpocon | _BV(3);
    opt_adc_write_reg(GPOCON_ADDR, gpocon);
}



// Reads 24 bits of raw data from the specified field, using the system of 5
// multiplexors with 8 pins each
uint32_t opt_adc_read_field_raw_data(uint8_t field_number) {
    uint8_t group = field_number / 8;
    uint8_t address = field_number % 8;

    // Enable the mux for the appropriate address
    // (this should turn on the LED and enable the amplifier)
    opt_adc_enable_mux(address + 1);
    _delay_ms(100);
    // TODO - set up and configure synchronous demodulator

    uint8_t adc_channel;
    switch (group) {
        case 0:
            adc_channel = 5;
            break;
        case 1:
            adc_channel = 7;
            break;
        case 2:
            adc_channel = 9;
            break;
        case 3:
            adc_channel = 11;
            break;
        case 4:
            adc_channel = 13;
            break;
        default:
            print("Unexpected sensor group\n");
            adc_channel = 5;
            break;
    }

    // Read ADC data and prepare to send it over SPI
    uint32_t raw_data = opt_adc_read_channel_raw_data(adc_channel, 1);
    opt_adc_disable_mux();

    return raw_data;
}




// TODO
/*
    // Calibrate the ADC and re-enable continuous conversion mode
    calibrate_adc(SYS_ZERO_SCALE_CALIB);
    calibrate_adc(SYS_FULL_SCALE_CALIB);
    enable_cont_conv_mode();

void opt_adc_calibrate(uint8_t mode_select_bits) {
    // TODO - NOT TESTED YET

    // Read from mode register
    uint32_t mode_data = opt_adc_read_reg(MODE_ADDR);

    // Clear and set mode select bits
    mode_data &= 0xff1fffff;
    mode_data |= ( ((uint32_t) mode_select_bits) << 21 );

    // Write to mode register
    opt_adc_write_reg(MODE_ADDR, mode_data);

    // Check the state of PB0 on the 32M1, which is MISO

    // Wait for _RDY_ to go high (when the calibration starts)
    while (!bit_is_set(PINB, PB0)){
        continue;
    }
    // Wait for _RDY_ to go low (when the calibration finishes)
    while (bit_is_set(PINB, PB0)){
        continue;
    }
}
*/
