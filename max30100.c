// Drivers/MAX30100/max30100.c
#include "max30100.h"

// Registers
#define REG_INT_STATUS   0x00
#define REG_INT_ENABLE   0x01
#define REG_FIFO_WR_PTR  0x02
#define REG_OVR_COUNTER  0x03
#define REG_FIFO_RD_PTR  0x04
#define REG_FIFO_DATA    0x05
#define REG_MODE_CONFIG  0x06
#define REG_SPO2_CONFIG  0x07
#define REG_LED_CONFIG   0x09
#define REG_TEMP_INT     0x16
#define REG_TEMP_FRAC    0x17
#define REG_REV_ID       0xFE
#define REG_PART_ID      0xFF

static I2C_HandleTypeDef* _hi2c;
static uint32_t ir_buffer[32];
static uint32_t red_buffer[32];
static uint8_t fifo_samples = 0;

static HAL_StatusTypeDef write_reg(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(_hi2c, MAX30100_I2C_ADDR, reg, 1, &val, 1, 100);
}

static HAL_StatusTypeDef read_reg(uint8_t reg, uint8_t* val)
{
    return HAL_I2C_Mem_Read(_hi2c, MAX30100_I2C_ADDR, reg, 1, val, 1, 100);
}

static HAL_StatusTypeDef read_fifo(uint8_t* data, uint16_t len)
{
    return HAL_I2C_Mem_Read(_hi2c, MAX30100_I2C_ADDR, REG_FIFO_DATA, 1, data, len, 200);
}

MAX30100_Status MAX30100_Init(I2C_HandleTypeDef* hi2c)
{
    _hi2c = hi2c;
    uint8_t part_id = 0;
    if (read_reg(REG_PART_ID, &part_id) != HAL_OK) return MAX30100_ERR;
    if (part_id != 0x11) {
        // Some clones differ; proceed cautiously
    }
    // Reset FIFO pointers
    write_reg(REG_FIFO_WR_PTR, 0x00);
    write_reg(REG_FIFO_RD_PTR, 0x00);
    write_reg(REG_OVR_COUNTER, 0x00);

    // Clear interrupts and enable HR/SPO2 ready
    write_reg(REG_INT_ENABLE, 0x10 | 0x20); // A_FULL, PPG_RDY
    return MAX30100_OK;
}

void MAX30100_SetMode(MAX30100_Mode mode)
{
    // Clear shutdown bit (bit7=0), set mode bits
    uint8_t val = mode & 0x03;
    write_reg(REG_MODE_CONFIG, val);
}

void MAX30100_SetLEDs(MAX30100_LEDCurrent red, MAX30100_LEDCurrent ir)
{
    uint8_t val = ((ir & 0x3F) << 6) | (red & 0x3F);
    write_reg(REG_LED_CONFIG, val);
}

void MAX30100_SetSpO2Config(uint8_t sample_rate, uint8_t adc_range, uint8_t led_pw)
{
    uint8_t val = adc_range | sample_rate | led_pw;
    write_reg(REG_SPO2_CONFIG, val);
}

void MAX30100_Update(void)
{
    // Read FIFO pointers
    uint8_t wr = 0, rd = 0;
    read_reg(REG_FIFO_WR_PTR, &wr);
    read_reg(REG_FIFO_RD_PTR, &rd);
    fifo_samples = (wr - rd) & 0x1F;

    while (fifo_samples--) {
        uint8_t raw[4]; // IR and RED are 16-bit each (MSB first)
        if (read_fifo(raw, 4) != HAL_OK) break;
        uint16_t ir = ((uint16_t)raw[0] << 8) | raw[1];
        uint16_t red = ((uint16_t)raw[2] << 8) | raw[3];
        // shift buffer, append newest
        for (int i = 31; i > 0; --i) {
            ir_buffer[i] = ir_buffer[i-1];
            red_buffer[i] = red_buffer[i-1];
        }
        ir_buffer[0] = ir;
        red_buffer[0] = red;
    }
}

void MAX30100_Compute(float* spo2, float* hr)
{
    // Very simplified placeholder algorithm:
    // Calculate AC/DC ratio and basic HR via zero-crossings or peak detection.
    // For production, use robust filters and calibration.
    float ir_mean = 0, red_mean = 0;
    for (int i = 0; i < 32; ++i) { ir_mean += ir_buffer[i]; red_mean += red_buffer[i]; }
    ir_mean /= 32.0f;
    red_mean /= 32.0f;

    float ir_ac = 0, red_ac = 0;
    for (int i = 0; i < 32; ++i) { ir_ac += (ir_buffer[i] - ir_mean)*(ir_buffer[i] - ir_mean);
                                   red_ac += (red_buffer[i] - red_mean)*(red_buffer[i] - red_mean); }
    ir_ac = (float)sqrtf(ir_ac / 32.0f);
    red_ac = (float)sqrtf(red_ac / 32.0f);

    float ratio = (red_ac / red_mean) / (ir_ac / ir_mean + 1e-6f);
    // Empirical linearization (placeholder): SpO2 ~ 110 - 25*ratio
    float spo2_est = 110.0f - 25.0f * ratio;
    if (spo2_est < 70.0f) spo2_est = 70.0f;
    if (spo2_est > 99.0f) spo2_est = 99.0f;
    *spo2 = spo2_est;

    // Heart rate via naive peak counting over last N samples; assume 100 Hz sample rate
    int peaks = 0;
    for (int i = 1; i < 31; ++i) {
        if (ir_buffer[i] > ir_buffer[i-1] && ir_buffer[i] > ir_buffer[i+1]) peaks++;
    }
    float bpm = peaks * 60.0f * (100.0f / 32.0f); // crude
    if (bpm < 40.0f) bpm = 40.0f;
    if (bpm > 200.0f) bpm = 200.0f;
    *hr = bpm;
}
