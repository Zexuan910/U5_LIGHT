#include "gh3018_soft_i2c.h"

#include "main.h"

#include <string.h>

#define GH3018_SOFT_I2C_HALF_PERIOD_US 5U
#define GH3018_SOFT_I2C_SCL_TIMEOUT_US 1000U
#define GH3018_SOFT_I2C_RECOVERY_CLOCKS 9U

static Gh3018SoftI2cStats s_stats;
static uint8_t s_dwtReady;

static void Gh3018SoftI2c_EnableCycleCounter(void)
{
    s_dwtReady = 0U;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    if ((DWT->CTRL & DWT_CTRL_NOCYCCNT_Msk) != 0U) {
        return;
    }

    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    __DSB();
    __ISB();

    uint32_t startCycles = DWT->CYCCNT;
    for (volatile uint32_t probe = 0U; probe < 16U; ++probe) {
        __NOP();
    }
    if (((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0U) &&
        (DWT->CYCCNT != startCycles)) {
        s_dwtReady = 1U;
    }
}

static uint32_t Gh3018SoftI2c_CyclesPerUs(void)
{
    uint32_t cyclesPerUs = HAL_RCC_GetHCLKFreq() / 1000000U;
    return (cyclesPerUs == 0U) ? 1U : cyclesPerUs;
}

void Gh3018SoftI2c_DelayUs(uint16_t usec)
{
    if (usec == 0U) {
        return;
    }

    if (s_dwtReady == 0U) {
        Gh3018SoftI2c_EnableCycleCounter();
    }

    uint32_t cyclesPerUs = Gh3018SoftI2c_CyclesPerUs();
    if (s_dwtReady != 0U) {
        uint32_t waitCycles = cyclesPerUs * (uint32_t)usec;
        uint32_t startCycles = DWT->CYCCNT;
        while ((uint32_t)(DWT->CYCCNT - startCycles) < waitCycles) {
            __NOP();
        }
        return;
    }

    volatile uint32_t fallbackLoops = ((cyclesPerUs * (uint32_t)usec) / 6U) + 1U;
    while (fallbackLoops-- > 0U) {
        __NOP();
    }
}

static void Gh3018SoftI2c_SdaLow(void)
{
    HAL_GPIO_WritePin(GH3018_I2C_SDA_GPIO_Port, GH3018_I2C_SDA_Pin, GPIO_PIN_RESET);
}

static void Gh3018SoftI2c_SdaRelease(void)
{
    HAL_GPIO_WritePin(GH3018_I2C_SDA_GPIO_Port, GH3018_I2C_SDA_Pin, GPIO_PIN_SET);
}

static void Gh3018SoftI2c_SclLow(void)
{
    HAL_GPIO_WritePin(GH3018_I2C_SCL_GPIO_Port, GH3018_I2C_SCL_Pin, GPIO_PIN_RESET);
}

static void Gh3018SoftI2c_SclRelease(void)
{
    HAL_GPIO_WritePin(GH3018_I2C_SCL_GPIO_Port, GH3018_I2C_SCL_Pin, GPIO_PIN_SET);
}

static uint8_t Gh3018SoftI2c_IsSdaHigh(void)
{
    return (HAL_GPIO_ReadPin(GH3018_I2C_SDA_GPIO_Port, GH3018_I2C_SDA_Pin) == GPIO_PIN_SET) ? 1U : 0U;
}

static uint8_t Gh3018SoftI2c_IsSclHigh(void)
{
    return (HAL_GPIO_ReadPin(GH3018_I2C_SCL_GPIO_Port, GH3018_I2C_SCL_Pin) == GPIO_PIN_SET) ? 1U : 0U;
}

static Gh3018SoftI2cStatus Gh3018SoftI2c_WaitSclHigh(void)
{
    Gh3018SoftI2c_SclRelease();

    uint32_t timeoutCycles = Gh3018SoftI2c_CyclesPerUs() * GH3018_SOFT_I2C_SCL_TIMEOUT_US;
    uint32_t startCycles = DWT->CYCCNT;
    while (Gh3018SoftI2c_IsSclHigh() == 0U) {
        if ((s_dwtReady != 0U) &&
            ((uint32_t)(DWT->CYCCNT - startCycles) >= timeoutCycles)) {
            s_stats.sclTimeoutCount++;
            return GH3018_SOFT_I2C_SCL_TIMEOUT;
        }

        if (s_dwtReady == 0U) {
            Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_SCL_TIMEOUT_US);
            if (Gh3018SoftI2c_IsSclHigh() == 0U) {
                s_stats.sclTimeoutCount++;
                return GH3018_SOFT_I2C_SCL_TIMEOUT;
            }
        }
    }

    return GH3018_SOFT_I2C_OK;
}

static Gh3018SoftI2cStatus Gh3018SoftI2c_Start(void)
{
    Gh3018SoftI2c_SdaRelease();
    Gh3018SoftI2cStatus status = Gh3018SoftI2c_WaitSclHigh();
    if (status != GH3018_SOFT_I2C_OK) {
        return status;
    }
    if (Gh3018SoftI2c_IsSdaHigh() == 0U) {
        return GH3018_SOFT_I2C_BUS_STUCK;
    }

    Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);
    Gh3018SoftI2c_SdaLow();
    Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);
    Gh3018SoftI2c_SclLow();
    Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);
    return GH3018_SOFT_I2C_OK;
}

static Gh3018SoftI2cStatus Gh3018SoftI2c_Stop(void)
{
    Gh3018SoftI2c_SdaLow();
    Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);

    Gh3018SoftI2cStatus status = Gh3018SoftI2c_WaitSclHigh();
    if (status != GH3018_SOFT_I2C_OK) {
        Gh3018SoftI2c_SdaRelease();
        return status;
    }

    Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);
    Gh3018SoftI2c_SdaRelease();
    Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);
    return (Gh3018SoftI2c_IsSdaHigh() != 0U) ? GH3018_SOFT_I2C_OK : GH3018_SOFT_I2C_BUS_STUCK;
}

static Gh3018SoftI2cStatus Gh3018SoftI2c_WriteByte(uint8_t value, uint8_t isAddress)
{
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
        if ((value & 0x80U) != 0U) {
            Gh3018SoftI2c_SdaRelease();
        } else {
            Gh3018SoftI2c_SdaLow();
        }
        value <<= 1;

        Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);
        Gh3018SoftI2cStatus status = Gh3018SoftI2c_WaitSclHigh();
        if (status != GH3018_SOFT_I2C_OK) {
            return status;
        }
        Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);
        Gh3018SoftI2c_SclLow();
    }

    Gh3018SoftI2c_SdaRelease();
    Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);
    Gh3018SoftI2cStatus status = Gh3018SoftI2c_WaitSclHigh();
    if (status != GH3018_SOFT_I2C_OK) {
        return status;
    }
    uint8_t acknowledged = (Gh3018SoftI2c_IsSdaHigh() == 0U) ? 1U : 0U;
    Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);
    Gh3018SoftI2c_SclLow();
    Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);

    if (acknowledged != 0U) {
        return GH3018_SOFT_I2C_OK;
    }
    if (isAddress != 0U) {
        s_stats.addressNackCount++;
        return GH3018_SOFT_I2C_ADDRESS_NACK;
    }

    s_stats.dataNackCount++;
    return GH3018_SOFT_I2C_DATA_NACK;
}

static Gh3018SoftI2cStatus Gh3018SoftI2c_ReadByte(uint8_t *value, uint8_t sendAck)
{
    uint8_t data = 0U;
    Gh3018SoftI2c_SdaRelease();

    for (uint8_t bit = 0U; bit < 8U; ++bit) {
        data <<= 1;
        Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);
        Gh3018SoftI2cStatus status = Gh3018SoftI2c_WaitSclHigh();
        if (status != GH3018_SOFT_I2C_OK) {
            return status;
        }
        if (Gh3018SoftI2c_IsSdaHigh() != 0U) {
            data |= 1U;
        }
        Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);
        Gh3018SoftI2c_SclLow();
    }

    if (sendAck != 0U) {
        Gh3018SoftI2c_SdaLow();
    } else {
        Gh3018SoftI2c_SdaRelease();
    }
    Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);
    Gh3018SoftI2cStatus status = Gh3018SoftI2c_WaitSclHigh();
    if (status != GH3018_SOFT_I2C_OK) {
        Gh3018SoftI2c_SdaRelease();
        return status;
    }
    Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);
    Gh3018SoftI2c_SclLow();
    Gh3018SoftI2c_SdaRelease();
    Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);

    *value = data;
    return GH3018_SOFT_I2C_OK;
}

static Gh3018SoftI2cStatus Gh3018SoftI2c_Finish(Gh3018SoftI2cStatus transferStatus)
{
    Gh3018SoftI2cStatus stopStatus = Gh3018SoftI2c_Stop();
    return (transferStatus == GH3018_SOFT_I2C_OK) ? stopStatus : transferStatus;
}

Gh3018SoftI2cStatus Gh3018SoftI2c_Init(void)
{
    memset(&s_stats, 0, sizeof(s_stats));
    s_dwtReady = 0U;
    Gh3018SoftI2c_EnableCycleCounter();

    Gh3018SoftI2c_SdaRelease();
    Gh3018SoftI2cStatus status = Gh3018SoftI2c_WaitSclHigh();
    if (status != GH3018_SOFT_I2C_OK) {
        return status;
    }
    Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);
    if (Gh3018SoftI2c_IsSdaHigh() != 0U) {
        return GH3018_SOFT_I2C_OK;
    }

    s_stats.busRecoveryCount++;
    for (uint8_t pulse = 0U; pulse < GH3018_SOFT_I2C_RECOVERY_CLOCKS; ++pulse) {
        Gh3018SoftI2c_SclLow();
        Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);
        status = Gh3018SoftI2c_WaitSclHigh();
        if (status != GH3018_SOFT_I2C_OK) {
            return status;
        }
        Gh3018SoftI2c_DelayUs(GH3018_SOFT_I2C_HALF_PERIOD_US);
        if (Gh3018SoftI2c_IsSdaHigh() != 0U) {
            break;
        }
    }

    status = Gh3018SoftI2c_Stop();
    if (status != GH3018_SOFT_I2C_OK) {
        return status;
    }
    return (Gh3018SoftI2c_IsSdaHigh() != 0U) ? GH3018_SOFT_I2C_OK : GH3018_SOFT_I2C_BUS_STUCK;
}

Gh3018SoftI2cStatus Gh3018SoftI2c_Probe(uint8_t address7bit)
{
    return Gh3018SoftI2c_Write(address7bit, NULL, 0U);
}

Gh3018SoftI2cStatus Gh3018SoftI2c_Write(uint8_t address7bit,
                                        const uint8_t data[],
                                        uint16_t dataLen)
{
    if ((address7bit > 0x7FU) || ((data == NULL) && (dataLen > 0U))) {
        return GH3018_SOFT_I2C_INVALID_ARG;
    }

    Gh3018SoftI2cStatus status = Gh3018SoftI2c_Start();
    if (status != GH3018_SOFT_I2C_OK) {
        return status;
    }

    status = Gh3018SoftI2c_WriteByte((uint8_t)(address7bit << 1), 1U);
    for (uint16_t index = 0U; (index < dataLen) && (status == GH3018_SOFT_I2C_OK); ++index) {
        status = Gh3018SoftI2c_WriteByte(data[index], 0U);
    }
    return Gh3018SoftI2c_Finish(status);
}

Gh3018SoftI2cStatus Gh3018SoftI2c_Read(uint8_t address7bit,
                                       uint8_t data[],
                                       uint16_t dataLen)
{
    if ((address7bit > 0x7FU) || ((data == NULL) && (dataLen > 0U)) || (dataLen == 0U)) {
        return GH3018_SOFT_I2C_INVALID_ARG;
    }

    Gh3018SoftI2cStatus status = Gh3018SoftI2c_Start();
    if (status != GH3018_SOFT_I2C_OK) {
        return status;
    }

    status = Gh3018SoftI2c_WriteByte((uint8_t)((address7bit << 1) | 1U), 1U);
    for (uint16_t index = 0U; (index < dataLen) && (status == GH3018_SOFT_I2C_OK); ++index) {
        uint8_t sendAck = (index + 1U < dataLen) ? 1U : 0U;
        status = Gh3018SoftI2c_ReadByte(&data[index], sendAck);
    }
    return Gh3018SoftI2c_Finish(status);
}

const Gh3018SoftI2cStats *Gh3018SoftI2c_GetStats(void)
{
    return &s_stats;
}
