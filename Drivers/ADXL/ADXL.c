#include "ADXL.h"

#define BUF_SIZE 4096U

/* Analog Devices ADXL345 (Rev G.): Register map */
#define DEVID 0x0
#define BW_RATE 0x2C
#define DATA_FORMAT 0x31
#define FIFO_CTL 0x38
#define FIFO_STATUS 0x39
#define DATA0 0x32
#define POWER_CTL 0x2D
#define THRESH_FF 0x28
#define TIME_FF 0x29

#define ADXL_DEVICE_ID 0xE5
#define ADXL_SPI_CMD_TIMEOUT_MS 10U
#define ADXL_CMD_SIZE 1U
#define ADXL_MAX_ADDR 63U
#define RECV_BUF_SIZE BUF_SIZE *ADXL_BYTES_PER_SAMPLE

#define ADXL_RAW_MIN (-4095)
#define ADXL_RAW_MAX (4095)

#define ADXL_WRITE_CMD_BIT (~0x80U)
#define ADXL_READ_CMD_BIT 0x80U
#define ADXL_MULTIREAD_BIT 0x40
#define ADXL_SINGLEREAD_BIT (~0x40)

#define ADXL_SPI_MODE_BIT 6U
#define ADXL_INT_MODE_BIT 5U
#define ADXL_JUSTIFY_BIT 2U
#define ADXL_RESOLUTION_BIT 3U

#define ADXL_LINKMODE_BIT (1 << 5)
#define ADXL_AUTOSLEEP_BIT (1 << 4)
#define ADXL_MEASURE_BIT (1 << 3)
#define ADXL_SLEEP_BIT (1 << 2)

#define ADXL_FIFO_MODE_BIT 6
#define ADXL_FIFO_MODE_STREAM 0x02U
#define ADXL_FIFO_SAMPLES 32U
#define FIFO_STATUS_SAMPLES_MASK 0x3FU

typedef struct
{
  float gain[ADXL_NUM_AXES];
  SPI_HandleTypeDef *hspi;
  GPIO_TypeDef *gpio_port;
  TIM_HandleTypeDef *timer;
  uint16_t gpio_pin;
  uint8_t buf[2][RECV_BUF_SIZE];
  uint16_t cur_idx;
  uint8_t buf_id;
  int8_t frame_ready;
} adxl_t;

static volatile adxl_t adxl = {.frame_ready = -1};

static adxl_status_t adxl_write_reg(uint8_t address, uint8_t value)
{
  if (address > ADXL_MAX_ADDR || adxl.hspi == NULL)
  {
    return ADXL_ERR;
  }

  address &= ADXL_WRITE_CMD_BIT;

  HAL_GPIO_WritePin(adxl.gpio_port, adxl.gpio_pin, GPIO_PIN_RESET);
  if (HAL_SPI_Transmit(adxl.hspi, &address, ADXL_CMD_SIZE, ADXL_SPI_CMD_TIMEOUT_MS) != HAL_OK)
  {
    HAL_GPIO_WritePin(adxl.gpio_port, adxl.gpio_pin, GPIO_PIN_SET);
    return ADXL_ERR;
  }

  if (HAL_SPI_Transmit(adxl.hspi, &value, ADXL_CMD_SIZE, ADXL_SPI_CMD_TIMEOUT_MS) != HAL_OK)
  {
    HAL_GPIO_WritePin(adxl.gpio_port, adxl.gpio_pin, GPIO_PIN_SET);
    return ADXL_ERR;
  }

  HAL_GPIO_WritePin(adxl.gpio_port, adxl.gpio_pin, GPIO_PIN_SET);
  return ADXL_OK;
}

static adxl_status_t adxl_read_reg(uint8_t address, uint8_t *value, uint8_t size)
{
  if (address > ADXL_MAX_ADDR || adxl.hspi == NULL)
  {
    return ADXL_ERR;
  }

  if (size > 1)
  {
    address |= ADXL_MULTIREAD_BIT;
  }
  else
  {
    address &= ADXL_SINGLEREAD_BIT;
  }

  address |= ADXL_READ_CMD_BIT;

  HAL_GPIO_WritePin(adxl.gpio_port, adxl.gpio_pin, GPIO_PIN_RESET);
  if (HAL_SPI_Transmit(adxl.hspi, &address, ADXL_CMD_SIZE, ADXL_SPI_CMD_TIMEOUT_MS) != HAL_OK)
  {
    HAL_GPIO_WritePin(adxl.gpio_port, adxl.gpio_pin, GPIO_PIN_SET);
    return ADXL_ERR;
  }

  if (HAL_SPI_Receive(adxl.hspi, value, size, ADXL_SPI_CMD_TIMEOUT_MS) != HAL_OK)
  {
    HAL_GPIO_WritePin(adxl.gpio_port, adxl.gpio_pin, GPIO_PIN_SET);
    return ADXL_ERR;
  }

  HAL_GPIO_WritePin(adxl.gpio_port, adxl.gpio_pin, GPIO_PIN_SET);
  return ADXL_OK;
}

bool adxl_is_clipped(int16_t v)
{
  return (v >= ADXL_RAW_MAX || v <= ADXL_RAW_MIN);
}

int16_t adxl_parse(const uint8_t *p)
{
  if (p == NULL)
    return 0;

  return (int16_t)(uint16_t)(p[0] | (p[1] << 8));
}

float adxl_get_scaled(int16_t v, uint8_t axis)
{
  if (axis >= ADXL_NUM_AXES)
    return 0.0f;

  return adxl.gain[axis] * v;
}

uint8_t *adxl_acquire_frame(void)
{
  int frame_ready = adxl.frame_ready;
  if (frame_ready >= 0)
  {
    adxl.frame_ready = -1;
    return adxl.buf[frame_ready];
  }
  else
  {
    return NULL;
  }
}

adxl_status_t adxl_init(const adxl_config_t *config)
{
  if (config == NULL || config->hspi == NULL || config->htim == NULL || config->spi_cs_port == NULL)
  {
    return ADXL_ERR;
  }

  adxl.hspi = config->hspi;
  adxl.gpio_port = config->spi_cs_port;
  adxl.gpio_pin = config->spi_cs_pin;
  adxl.timer = config->htim;

  HAL_GPIO_WritePin(adxl.gpio_port, adxl.gpio_pin, GPIO_PIN_RESET);

  uint8_t device_id = 0;
  if (adxl_read_reg(DEVID, &device_id, ADXL_SPI_CMD_TIMEOUT_MS) != ADXL_OK)
    return ADXL_ERR;

  if (device_id != ADXL_DEVICE_ID)
  {
    HAL_GPIO_WritePin(adxl.gpio_port, adxl.gpio_pin, GPIO_PIN_SET);
    return ADXL_ERR;
  }

  uint8_t bwreg = 0;
  if ((config->rate < ADXL_RATE_6_25) || (config->rate > ADXL_RATE_3200))
  {
    return ADXL_ERR;
  }

  bwreg |= (config->rate);

  if (adxl_write_reg(BW_RATE, bwreg) != ADXL_OK)
    return ADXL_ERR;

  uint8_t format_reg = 0;
  uint8_t int_mode = 0;
  uint8_t justify = 0;

  format_reg = (config->spi_mode << ADXL_SPI_MODE_BIT) | (int_mode << ADXL_INT_MODE_BIT) |
               (justify << ADXL_JUSTIFY_BIT) | (config->resolution << ADXL_RESOLUTION_BIT);
  format_reg |= config->range;
  if (adxl_write_reg(DATA_FORMAT, format_reg) != ADXL_OK)
    return ADXL_ERR;

  if (config->resolution == ADXL_RESOLUTION_10BIT)
  {
    switch (config->range)
    {
    case ADXL_RANGE_2G:
      adxl.gain[0] = adxl.gain[1] = adxl.gain[2] = 1 / 255.0f;
      break;
    case ADXL_RANGE_4G:
      adxl.gain[0] = adxl.gain[1] = adxl.gain[2] = 1 / 127.0f;
      break;
    case ADXL_RANGE_8G:
      adxl.gain[0] = adxl.gain[1] = adxl.gain[2] = 1 / 63.0f;
      break;
    case ADXL_RANGE_16G:
      adxl.gain[0] = adxl.gain[1] = adxl.gain[2] = 1 / 31.0f;
      break;
    }
  }
  else
  {
    adxl.gain[0] = adxl.gain[1] = adxl.gain[2] = 1 / 255.0f;
  }

  uint8_t reg = 0;
  reg &= ~ADXL_AUTOSLEEP_BIT;
  reg &= ~ADXL_LINKMODE_BIT;
  if (adxl_write_reg(POWER_CTL, reg) != ADXL_OK)
    return ADXL_ERR;

  uint8_t fifo_reg = (ADXL_FIFO_MODE_STREAM << ADXL_FIFO_MODE_BIT) |
                     ADXL_FIFO_SAMPLES;
  if (adxl_write_reg(FIFO_CTL, fifo_reg) != ADXL_OK)
    return ADXL_ERR;

  return ADXL_OK;
}

adxl_status_t adxl_enable(void)
{
  uint8_t reg;
  if (adxl_read_reg(POWER_CTL, &reg, 1) != ADXL_OK)
    return ADXL_ERR;

  reg &= ~ADXL_SLEEP_BIT;
  reg |= ADXL_MEASURE_BIT;
  if (adxl_write_reg(POWER_CTL, reg) != ADXL_OK)
    return ADXL_ERR;

  if (HAL_TIM_Base_Start_IT(adxl.timer) != HAL_OK)
  {
    return ADXL_ERR;
  }

  return ADXL_OK;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (adxl.timer == NULL || htim->Instance != adxl.timer->Instance)
  {
    return;
  }

  if (adxl.hspi->State != HAL_SPI_STATE_READY)
  {
    return;
  }

  uint8_t fifo_status;
  if (adxl_read_reg(FIFO_STATUS, &fifo_status, ADXL_SPI_CMD_TIMEOUT_MS) != ADXL_OK)
    return;

  uint8_t entries = fifo_status & FIFO_STATUS_SAMPLES_MASK;
  uint16_t room = (RECV_BUF_SIZE - adxl.cur_idx) / ADXL_BYTES_PER_SAMPLE;
  if (entries > room)
  {
    entries = room;
  }

  if (entries == 0)
  {
    return;
  }

  for (uint8_t i = 0; i < entries; i++)
  {
    if (adxl_read_reg(DATA0, &adxl.buf[adxl.buf_id][adxl.cur_idx], ADXL_BYTES_PER_SAMPLE) != ADXL_OK)
      return;

    adxl.cur_idx += ADXL_BYTES_PER_SAMPLE;

    if (adxl.cur_idx >= RECV_BUF_SIZE)
    {
      if (adxl.frame_ready < 0)
      {
        adxl.frame_ready = adxl.buf_id;
        adxl.buf_id ^= 1;
      }
      adxl.cur_idx = 0;
    }
  }
}