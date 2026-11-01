#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include <stdbool.h>

bool logger_is_busy(void);

void logger_log_serial(const uint8_t *frame, uint16_t size);

#endif /* LOGGER_H */