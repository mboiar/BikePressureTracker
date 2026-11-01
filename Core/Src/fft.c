#include "fft.h"
#include "ADXL.h"
#include "arm_math.h"
#include "stdint.h"

#define FFT_SIZE 4096U
#define FFT_MAGIC 0x31544646 /* 'F' 'F' 'T' '1' */

typedef struct
{
    uint16_t npeaks;
    float min_peak;
    uint16_t guard;
    uint16_t min_bin;
    arm_rfft_fast_instance_f32 rfft;
    float32_t hann[FFT_SIZE], fft_in[FFT_SIZE], fft_out[FFT_SIZE];
    float32_t amp_scale;
    float mag[FFT_SIZE / 2];
    fft_pkt_t pkt;
} fft_t;

static fft_t fft;

fft_status_t fft_init(const fft_config_t *config)
{
    if (config == NULL)
    {
        return FFT_ERR;
    }

    arm_status fft_status = arm_rfft_fast_init_f32(&fft.rfft, FFT_SIZE);
    if (fft_status != ARM_MATH_SUCCESS)
    {
        return FFT_ERR;
    }

    float sum = 0;
    for (int i = 0; i < FFT_SIZE; i++)
    {
        fft.hann[i] = 0.5f * (1.0f - arm_cos_f32(2 * PI * i / (FFT_SIZE - 1)));
        sum += fft.hann[i];
    }

    fft.amp_scale = 2.0f / sum; // single-sided amplitude, window-gain corrected
    fft.pkt.fs_hz = config->freq_sampling;
    fft.npeaks = NPEAKS;
    fft.guard = config->guard;

    if (config->min_bin > FFT_SIZE / 2)
    {
        return FFT_ERR;
    }

    fft.min_bin = config->min_bin;
    fft.min_peak = config->min_peak;

    fft.pkt.magic = FFT_MAGIC;

    return FFT_OK;
}

static void pick_peaks(float *m, peak_t *out)
{
    for (int k = 0; k < fft.npeaks; k++)
    {
        float v;
        uint32_t idx;
        arm_max_f32(&m[fft.min_bin], FFT_SIZE / 2 - fft.min_bin - 1, &v, &idx);
        idx += fft.min_bin;
        if (v < fft.min_peak)
        {
            out[k].f_hz = 0;
            out[k].amp_g = 0;
            continue;
        }

        float a = m[idx - 1], c = m[idx + 1]; // parabolic interpolation
        float den = a - 2.0f * v + c;
        float d = (den != 0.0f) ? 0.5f * (a - c) / den : 0.0f;
        out[k].f_hz = (idx + d) * (float)fft.pkt.fs_hz / FFT_SIZE;
        out[k].amp_g = v - 0.25f * (a - c) * d;

        for (int j = (int)idx - fft.guard; j <= (int)idx + fft.guard; j++)
            if (j >= 0 && j < FFT_SIZE / 2)
                m[j] = 0.0f;
    }
}

void fft_process_frame(const uint8_t *frame, fft_pkt_t *pkt)
{
    if (frame == NULL || pkt == NULL)
    {
        return;
    }

    for (int ax = 0; ax < ADXL_NUM_AXES; ax++)
    {
        const volatile uint8_t *p = &frame[ax * 2];
        float mean = 0;
        uint16_t clipped = 0;
        for (int i = 0; i < FFT_SIZE; i++, p += ADXL_BYTES_PER_SAMPLE)
        {
            int16_t v = adxl_parse(p);
            if (adxl_is_clipped(v))
            {
                clipped++;
            }

            fft.fft_in[i] = adxl_get_scaled(v, ax);
            mean += fft.fft_in[i];
        }
        mean /= FFT_SIZE;
        for (int i = 0; i < FFT_SIZE; i++)
            fft.fft_in[i] = (fft.fft_in[i] - mean) * fft.hann[i];

        arm_rfft_fast_f32(&fft.rfft, fft.fft_in, fft.fft_out, 0);
        arm_cmplx_mag_f32(fft.fft_out, fft.mag, FFT_SIZE / 2);
        arm_scale_f32(fft.mag, fft.amp_scale, fft.mag, FFT_SIZE / 2);
        fft.mag[0] = 0.0f;
        pick_peaks(fft.mag, fft.pkt.peak[ax]);

        fft.pkt.clip[ax] = clipped;
    }

    *pkt = fft.pkt;

    fft.pkt.frame++;
}
