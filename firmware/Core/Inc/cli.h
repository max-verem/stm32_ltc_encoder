#ifndef CLI_H
#define CLI_H

#include <stdint.h>

void cli_idle();
void cli_callback_recv_data(uint8_t* buf, uint32_t len);
void cli_callback_sent_data(uint8_t* buf, uint32_t len);

#endif /* CLI_H */
