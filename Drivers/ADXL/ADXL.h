#ifndef ADXL_H
#define ADXL_H

#include "stm32f4xx.h"
#include "stdbool.h"

#define ADXL_NUM_AXES 3U
#define ADXL_BYTES_PER_SAMPLE 6U

typedef enum
{
  ADXL_OK,
  ADXL_ERR
} adxl_status_t;

typedef enum
{
  ADXL_SPI_4WIRE,
  ADXL_SPI_3WIRE
} adxl_spi_mode_t;

typedef enum
{
  ADXL_RATE_6_25 = 6,
  ADXL_RATE_12_5,
  ADXL_RATE_25,
  ADXL_RATE_50,
  ADXL_RATE_100,
  ADXL_RATE_200,
  ADXL_RATE_400,
  ADXL_RATE_800,
  ADXL_RATE_1600,
  ADXL_RATE_3200
} adxl_rate_t;

typedef enum
{
  ADXL_RESOLUTION_10BIT,
  ADXL_RESOLUTION_13BIT
} adxl_resolution_t;

typedef enum
{
  ADXL_RANGE_2G,
  ADXL_RANGE_4G,
  ADXL_RANGE_8G,
  ADXL_RANGE_16G
} adxl_range_t;

typedef struct
{
  SPI_HandleTypeDef *hspi;
  TIM_HandleTypeDef *htim;
  GPIO_TypeDef *spi_cs_port;
  uint16_t spi_cs_pin;
  uint8_t spi_mode;
  uint8_t rate;
  uint8_t range;
  uint8_t resolution;
} adxl_config_t;

/** @brief Initialize ADXL345 driver.
 *  @param adxl config struct
 *
 *  @return ADXL_ERR on error, ADXL_OK otherwise
 */
adxl_status_t adxl_init(const adxl_config_t *adxl);

/** @brief Enable and start measuring. */
adxl_status_t adxl_enable(void);

/** @brief Check if raw ADXL value was clipped (out-of-range).
 *
 * @param v raw ADXL value
 *
 * @retval true if clipped, otherwise false
 */
bool adxl_is_clipped(int16_t v);

/** @brief Parse ADXL buffer into a raw integer value.
 *
 * @param p data buffer
 *
 * @return raw ADXL measurement
 */
int16_t adxl_parse(const uint8_t *p);

/** @brief Get scaled measurement.
 *
 * @param v raw ADXL measurement
 * @param axis measurement axis 0-X, 1-Y, 2-Z
 *
 * @return scaled measurement
 */
float adxl_get_scaled(int16_t v, uint8_t axis);

uint8_t *adxl_acquire_frame(void);

#endif /* ADXL_H */
