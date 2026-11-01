#ifndef FFT_H
#define FFT_H

#include <ADXL.h>
#include <stdint.h>

#define NPEAKS 8U

typedef enum
{
    FFT_OK,
    FFT_ERR
} fft_status_t;

typedef struct
{
    float f_hz;
    float amp_g;
} peak_t;

typedef struct
{
    uint32_t magic;
    uint32_t frame;
    uint16_t fs_hz;
    uint16_t clip[ADXL_NUM_AXES];
    peak_t peak[3][NPEAKS];
} fft_pkt_t;

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
void fft_process_frame(const uint8_t *frame, fft_pkt_t *pkt);

#endif /* FFT_H */