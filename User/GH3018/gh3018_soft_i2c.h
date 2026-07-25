#ifndef GH3018_SOFT_I2C_H
#define GH3018_SOFT_I2C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GH3018_SOFT_I2C_OK = 0,
    GH3018_SOFT_I2C_INVALID_ARG,
    GH3018_SOFT_I2C_BUS_STUCK,
    GH3018_SOFT_I2C_SCL_TIMEOUT,
    GH3018_SOFT_I2C_ADDRESS_NACK,
    GH3018_SOFT_I2C_DATA_NACK
} Gh3018SoftI2cStatus;

typedef struct {
    uint32_t busRecoveryCount;
    uint32_t addressNackCount;
    uint32_t dataNackCount;
    uint32_t sclTimeoutCount;
} Gh3018SoftI2cStats;

Gh3018SoftI2cStatus Gh3018SoftI2c_Init(void);
Gh3018SoftI2cStatus Gh3018SoftI2c_Probe(uint8_t address7bit);
Gh3018SoftI2cStatus Gh3018SoftI2c_Write(uint8_t address7bit,
                                        const uint8_t data[],
                                        uint16_t dataLen);
Gh3018SoftI2cStatus Gh3018SoftI2c_Read(uint8_t address7bit,
                                       uint8_t data[],
                                       uint16_t dataLen);
void Gh3018SoftI2c_DelayUs(uint16_t usec);
const Gh3018SoftI2cStats *Gh3018SoftI2c_GetStats(void);

#ifdef __cplusplus
}
#endif

#endif
