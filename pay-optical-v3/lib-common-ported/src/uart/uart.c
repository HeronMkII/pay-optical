/*
UART (TX and RX) library.

32M1/64M1 datasheet: https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8209-8-bit%20AVR%20ATmega16M1-32M1-64M1_Datasheet.pdf

UART is a protocol that allows devices to send data to each
other one byte at a time (usually represented as one character in a
user-friendly terminal).
*/

#include <uart/uart.h>
#include <string.h>

// Maximum number of characters the UART RX buffer can store
#define UART_MAX_RX_BUF_SIZE 50

// Buffer of received characters
volatile uint8_t uart_rx_buf[UART_MAX_RX_BUF_SIZE];
// Number of valid characters in buffer (starting at index 0)
volatile uint8_t uart_rx_buf_count;

// default rx callback (no operation)
uint8_t _uart_rx_cb_nop(const uint8_t* c, uint8_t len) {
    return 0;
}
// Global RX callback function
uart_rx_cb_t uart_rx_cb = _uart_rx_cb_nop;




// Initializes the UART library with the default 9600 baud rate.
void init_uart(void) {
    // Set software reset bit (this bit will self-reset afer, p. 290)
    LINCR = _BV(LSWRES);

    // Set default baud rate
    set_uart_baud_rate(UART_DEF_BAUD_RATE);

    /*
    Enable UART, full duplex (p. 290)
    No reset
    LIN 2.1
    8-bit, no parity, listen mode off
    Enable UART
    Enable RX byte and TX byte
    */
    LINCR = _BV(LENA) | _BV(LCMD2) | _BV(LCMD1) | _BV(LCMD0);

    // Only enable interrupts for received charcaters (p. 294)
    LINENIR = _BV(LENRXOK);

    // reset RX buffer and counter
    clear_uart_rx_buf();
    // Set default (no operation) RX callback
    uart_rx_cb = _uart_rx_cb_nop;

    // globally enable interrupts
    sei();
}

/*
Sets the values of the registers that determine the UART baud rate.

lbt - value of LBT[5:0] in LINBTR (LIN bit timing) register (AFTER subtracting 1
    in the formula); number of samples per bit; must be between 8 and 63
    (p. 283, 289, 297)

ldiv - Scaling of clk_io frequency; value to assign to 16-bit LINBRR register
    (p. 298); calculated from formula on p.282; this number is not necessarily
    integer; if the number is too far from being an integer, too much error
    will accumulate and UART will outputgarbage
    from p.282: LDIV = (F_IO / (BAUD_RATE * BIT_SAMPLES)) - 1;
*/
static void set_baud_regs(uint8_t lbt, uint16_t ldiv) {
    // Set these values atomically because the baud rate is determined by both
    // Also, we are setting LINBRR, which is a 16-bit register
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        // To be able to set LDISR and LBT[5:0], need to set LENA = 0 (p. 283)
        // Save the old value of LINCR
        uint8_t LINCR_old = LINCR;
        LINCR &= ~_BV(LENA);

        // Need to set the LDISR bit to 1 (disable bit timing resynchronization)
        // to be able to set LBT[5:0] (p. 289, 297)
        LINBTR = _BV(LDISR) | lbt;

        // Restore the old value of LINCR
        LINCR = LINCR_old;

        // Set LINBRR 16-bit register to LDIV (high and low registers separate)
        LINBRRH = (uint8_t) (ldiv >> 8);
        LINBRRL = (uint8_t) ldiv;
    }
}

/*
Sets the UART baud rate on the microcontroller.
baud_rate - one of a select set of baud rates where the clock division ratios
    are known to work
The baud rate needs to match the one used for the transceiver/CoolTerm.
*/
void set_uart_baud_rate(uart_baud_rate_t baud_rate) {
    // These values of LBT and LDIV are chosen to give as close to integer
    // numbers as possible with the formula on p. 282
    // Trying to keep LBT close to the default 32
    switch (baud_rate) {
        case UART_BAUD_1200:
            set_baud_regs(32, 207);
            break;
        case UART_BAUD_9600:
            set_baud_regs(32, 25);
            break;
        case UART_BAUD_19200:
            set_baud_regs(32, 12);
            break;
        case UART_BAUD_115200:
            set_baud_regs(35, 1);
            break;
        default:
            break;
    }
}

/*
Sends one character over UART (TX)
c - character to send
*/
void put_uart_char(uint8_t c) {
    uint16_t timeout = UINT16_MAX;
    while ((LINSIR & _BV(LBUSY)) && timeout--);
    LINDAT = c;
}

/*
Gets one (received) character from UART (RX).
c - will be set by this function to the received character
TODO - Maybe change to uint8_t get_uart_char(uint8_t*) and write value directly?
Frees up ret val for error handling
*/
void get_uart_char(uint8_t* c) {
    uint16_t timeout = UINT16_MAX;
    while ((LINSIR & _BV(LBUSY)) && timeout--);
    *c = LINDAT;
}

/*
Sends a sequence of characters over UART.
msg - pointer to start of array
len - number of characters
*/
void send_uart(const uint8_t* msg, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        put_uart_char(msg[i]);
    }
}

/*
Sets the callback function that will be called when UART receives data.
cb - callback function

The function must have the following signature:
uint8_t func(const uint8_t* data, uint8_t len);

The UART library will pass a pointer to the array (data) and the number of characters (len).
The function can process the characters however it wants and must return the
number of characters it has "processed", which will then be removed from the
buffer of received UART characters.
*/
void set_uart_rx_cb(uart_rx_cb_t cb) {
    uart_rx_cb = cb;
}

/*
Gets the number of characters that are currently in the UART RX buffer but have
not been processed yet.
*/
uint8_t get_uart_rx_buf_count(void) {
    return uart_rx_buf_count;
}

/*
Clears the RX buffer (sets all values in the array to 0, sets counter to 0).
*/
void clear_uart_rx_buf(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        uart_rx_buf_count = 0;
        for (uint8_t i = 0; i < UART_MAX_RX_BUF_SIZE; i++) {
            uart_rx_buf[i] = 0;
        }
    }
}

// Interrupt handler that will be called when we receive a character over UART
ISR(LIN_TC_vect) {
    // Check if we got the interrupt for a received character (p. 293)
    if (LINSIR & _BV(LRXOK)) {
        // Fetch the new recieved character
        static uint8_t c;
        get_uart_char(&c);

        // Add the new character to the RX buffer
        uart_rx_buf[uart_rx_buf_count] = c;
        uart_rx_buf_count += 1;

        /*
        Call the RX callback function to process the character buffer
        It's fine to cast the buffer pointer to non-volatile, because we are in
        an interrupt handler so the callback function can't be interrupted
        (i.e. the contents of uart_rx_buf can't change)
        */
        uint8_t read_bytes = uart_rx_cb(
            (const uint8_t*) uart_rx_buf, uart_rx_buf_count);

        // If some number of bytes were read, shift everything in the buffer
        // leftward by the number of bytes read
        if (read_bytes > 0) {
            uart_rx_buf_count -= read_bytes;

            if (uart_rx_buf_count > 0) {
                // parameters - destination, source, number of bytes
                memmove((void*) uart_rx_buf, (void*) (uart_rx_buf + read_bytes),
                    uart_rx_buf_count);
            }
        }

        // If the buffer is full, clear it
        if (uart_rx_buf_count >= UART_MAX_RX_BUF_SIZE) {
            clear_uart_rx_buf();
        }

        // Clear RX interrupt bit (p. 293)
        // TODO - does this actually work? should we write a 1 instead?
        LINSIR &= ~_BV(LRXOK);
    }
}
