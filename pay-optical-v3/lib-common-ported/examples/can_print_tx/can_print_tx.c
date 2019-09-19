#include <uart/uart.h>
#include <can/can.h>

void tx_callback(uint8_t*, uint8_t*);

mob_t tx_mob = {
    .mob_num = 0,
    .mob_type = TX_MOB,
    .id_tag = { 0x0000 },
    .ctrl = default_tx_ctrl,
    .tx_data_cb = tx_callback
};

void tx_callback(uint8_t* data, uint8_t* len) {
    *len = 7;
    char str[] = "Hello!";

    for(uint8_t i = 0; i < *len; i++) {
        data[i] = str[i];
    }
}

int main(void) {
    init_uart();
    print("UART initialized\n");

    init_can();
    init_tx_mob(&tx_mob);

    while (1) {
        resume_mob(&tx_mob);
        while (!is_paused(&tx_mob)) {};

        print("Status: %#02x\n", mob_status(&tx_mob));
        print("Tx error count: %d\n", CANTEC);
    }
}
