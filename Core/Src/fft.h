#ifndef FFT_H
#define FFT_H

#include <stdint.h>

typedef enum
{
    FFT_OK,
    FFT_ERR
} fft_status_t;

typedef struct
{
    uint16_t freq_sampling; /* Sampling frequency */
    uint16_t min_bin;
    uint16_t guard;
    float min_peak; /* Mininum peak amplitude */
} fft_config_t;

/** @brief Initialize FFT module.
 *  @param config FFT config
 *  @retval FFT_ERR on error, otherwise FFT_OK
 */
fft_status_t fft_init(const fft_config_t *config);

/** @brief Process data frame.
 *  @param frame data buffer
 */
void fft_process_frame(const uint8_t *frame);

#endif /* FFT_H */