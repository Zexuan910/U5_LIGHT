/*
*File Name: hbd_ctrl.h
************************************************************************
*
* Copyright 2018 - 2020 All rights reserved. Goodix Inc.
*
* Description: GH30x sensor control library header.
* Note:    
* Identifier abbreviation:  Wnd - Window
*                           Msg - Message        
**********************************************************************/

#ifndef __HBD_CTRL_H__
#define __HBD_CTRL_H__

#define __HBD_BPF_ENABLE__    (0)
#define ENABLE_SPO2_ABNORMAL_STATE    (0)


typedef unsigned char GU8;
typedef signed char GS8;
typedef unsigned short GU16;
typedef signed short GS16;

typedef int GS32;
typedef unsigned int GU32;
typedef unsigned long long   GU64;
typedef signed long long GS64;
typedef float GF32;
typedef unsigned char GBOOL;
#define BYTE_TRUE       (GU8)1
#define BYTE_FALSE      (GU8)0

/* Hbd functiional state enum. */
typedef enum 
{
    HBD_FUNCTIONAL_STATE_DISABLE = 0, 
    HBD_FUNCTIONAL_STATE_ENABLE = 1,
} EM_HBD_FUNCTIONAL_STATE;

/* Int status enum. */
enum EM_INT_STATUS
{
    INT_STATUS_CHIP_RESET = 0,
    INT_STATUS_NEW_DATA,
    INT_STATUS_FIFO_WATERMARK,
    INT_STATUS_FIFO_FULL,
    INT_STATUS_WEAR_DETECTED,
    INT_STATUS_UNWEAR_DETECTED,
    INT_STATUS_INVALID,
} ;

/* I2c Low Two bit select enum. */
enum EM_HBD_I2C_ID_SEL
{
    HBD_I2C_ID_SEL_1L0L = 0,
    HBD_I2C_ID_SEL_1L0H,
    HBD_I2C_ID_SEL_1H0L,
    HBD_I2C_ID_SEL_1H0H,
    HBD_I2C_ID_INVALID,
};

/* Led pd select enum. */
typedef enum 
{
    HBD_LED_PD_SEL_INTERNAL = 0,
    HBD_LED_PD_SEL_EXTERNAL,
} EM_HBD_LED_PD_SEL;

/* Led logic channel map to hw led enum. */
typedef enum
{
    HBD_LED_LOGIC_CHANNEL_MAP_PHY012 = 0,
    HBD_LED_LOGIC_CHANNEL_MAP_PHY021,
    HBD_LED_LOGIC_CHANNEL_MAP_PHY102,
    HBD_LED_LOGIC_CHANNEL_MAP_PHY120,
    HBD_LED_LOGIC_CHANNEL_MAP_PHY201,
    HBD_LED_LOGIC_CHANNEL_MAP_PHY210,
} EM_HBD_LED_LOGIC_CHANNEL_MAP;

/* sample time enum. */
typedef enum 
{
    HBD_SAMPLE_TIME_32CYCLES = 0,
    HBD_SAMPLE_TIME_64CYCLES,
    HBD_SAMPLE_TIME_128CYCLES,
    HBD_SAMPLE_TIME_256CYCLES,
    HBD_SAMPLE_TIME_512CYCLES,
    HBD_SAMPLE_TIME_1024CYCLES,
    HBD_SAMPLE_TIME_2048CYCLES,
} EM_HBD_SAMPLE_TIME;

/* tia gain enum. */
typedef enum
{
    HBD_TIA_GAIN_0 = 0,
    HBD_TIA_GAIN_1,
    HBD_TIA_GAIN_2,
    HBD_TIA_GAIN_3,
    HBD_TIA_GAIN_4,
    HBD_TIA_GAIN_5,
    HBD_TIA_GAIN_6,
    HBD_TIA_GAIN_7,
} EM_HBD_TIA_GAIN;

/* G-sensor sensitivity(counts/g) enum. */
typedef enum
{
    HBD_GSENSOR_SENSITIVITY_512_COUNTS_PER_G = 0, 
    HBD_GSENSOR_SENSITIVITY_1024_COUNTS_PER_G,    
    HBD_GSENSOR_SENSITIVITY_2048_COUNTS_PER_G,
    HBD_GSENSOR_SENSITIVITY_4096_COUNTS_PER_G,
    HBD_GSENSOR_SENSITIVITY_8192_COUNTS_PER_G,
} EM_HBD_GSENSOR_SENSITIVITY;

/* Hb config struct type. */
typedef struct
{
    EM_HBD_FUNCTIONAL_STATE emHbModeFifoEnable;     // HB fifo enable flag
    EM_HBD_FUNCTIONAL_STATE emHrvModeFifoEnable;    // HRV fifo enable flag
    EM_HBD_FUNCTIONAL_STATE emBpfModeFifoEnable;     // BPF fifo enable flag
    EM_HBD_FUNCTIONAL_STATE emReserve2ModeFifoEnable;     // Reserve fifo enable flag
    EM_HBD_FUNCTIONAL_STATE emReserve3ModeFifoEnable;     // Reserve fifo enable flag
    EM_HBD_FUNCTIONAL_STATE emSpo2ModeFifoEnable;     // Spo2 fifo enable flag
} ST_HB_CONFIG_TYPE;

/* Adt config struct type. Notes: only logic channel0 & channel1 valid in adt mode. */
typedef struct
{
    EM_HBD_FUNCTIONAL_STATE emGINTEnable;                  // GINT enable flag
    EM_HBD_LED_LOGIC_CHANNEL_MAP emLedLogicChannelMap;     // Logic map
    EM_HBD_LED_PD_SEL emLogicChannel0PDSelect;             // Logic Channel0 PD select
    EM_HBD_LED_PD_SEL emLogicChannel1PDSelect;             // Logic Channel1 PD select
    GU8 uchLogicChannel0Current;                           // Logic Channel0 current cofig(1 step = 400uA)
    GU8 uchLogicChannel1Current;                           // Logic Channel1 current cofig(1 step = 400uA)
    EM_HBD_TIA_GAIN emLogicChannel0TiaGain;                // Logic Channel0 tia gain(valid value:0-7)
    EM_HBD_TIA_GAIN emLogicChannel1TiaGain;                // Logic Channel1 tia gain(valid value:0-7)
    EM_HBD_SAMPLE_TIME emSampleTime;                       // Sample time
} ST_ADT_CONFIG_TYPE;

/* Hbd init config struct type. */
typedef struct
{
    ST_HB_CONFIG_TYPE stHbInitConfig;
    ST_ADT_CONFIG_TYPE stAdtInitConfig;
} ST_HBD_INIT_CONFIG_TYPE;

/* Gsensor data struct type. */
typedef struct
{
    GS16 sXAxisVal; // X-Axis Value
    GS16 sYAxisVal; // Y-Axis Value
    GS16 sZAxisVal; // Z-Axis Value
} ST_GS_DATA_TYPE;

/* register struct. */
typedef struct  
{
    GU16 usRegAddr;
    GU16 usRegData;
} ST_REGISTER;
 
 /* autoled  channel struct. */
typedef struct
{
    GU32 unTrigerUpperThr;
    GU32 unTrigerLowerThr;
     
    GU32 unTargetUpperThr;
    GU32 unTargetLowerThr; 
    GU32 unCompatibleTakeoffThr; 
    
    EM_HBD_FUNCTIONAL_STATE emAutoledEnable;
    
    GU8 uchGainLimitThr;          //Gain Max 7,will be changed if strong light
	GU8 uchCurrentLimit;          //Energy Saving Setting,Can be changed by diffrent mode such as HB SPO2
	EM_HBD_FUNCTIONAL_STATE emAutoledSaveEnergyEnable;  //If Enable,AutoLED will start from little current, The value changed by Reg 0x2A19
} ST_AUTO_LED_CH_CONFIG;

typedef struct 
{
    GU8 uchHbValue; //hb value
    GU8 uchAccuracyLevel; //hb accuracy level
    GU8 uchWearingState; //1,wear;2,unwear
    GU8 uchWearingQuality; //wear accuracy level
    GU8 uchSNR; //SNR(1-100)
    GU8 uchScene; //cuurent scene
    GU8 uchMotionState; //motion state:0/1/2 means small/medium/big
    GU8 uchSleepFlag;
} ST_HB_RES;

typedef struct 
{
    GU16 usRRvalueArr[4];
    GU8 uchRRvalueCnt;
    GU8 uchHrvConfidentLvl;
} ST_HRV_RES;

/*
 *uliSpo2InvlaidFlag ：      bit0 = SPORT_ABNORMALATION_STATE
                            bit1 = DIRECTION_ABNORMALATION_STATE
                            bit2 = AUTOLED_ABNORMALATION_STATE
                            bit3 = R_VALUE_INVALID_STATE
*/
typedef enum
{
    SPORT_ABNORMALATION_STATE = 0x01,
    DIRECTION_ABNORMALATION_STATE = 0x02,
    AUTOLED_ABNORMALATION_STATE = 0X40,
    R_VALUE_INVALID_STATE =0x80
}EMSpo2InvalidFlag;

typedef struct 
{
    GU8 uchWearingState;//1,wear;2,unwear
    GU8 uchSpo2;//spo2 value
    GF32 fSpo2;// spo2 value (with Decimal)
    GU16 usSpo2RVal;//R value
    GS32 uchSpo2ValidLvl;
    GU8 uchSpo2Confidence;//range 0~100
    GS32 uliSpo2InvlaidFlag;
    GU8 uchHbValue;//heart rate
    GU8 uchHbConfidentLvl;//range 0~100
    GU16 usHrvRRVal[4];//RRI
    GU8 uchHrvcnt;//valid RRI, max 4
    GU8 uchHrvConfidentLvl;
} ST_SPO2_RES;

#if (__HBD_BPF_ENABLE__)
typedef struct 
{
    GU8 uchWearingState;//1,wear;2,unwear
    GU8 uchResultNum;//output result num, range 0~4
    GU8 uchConfidence;//range 0~5
    GU8 uchAFib;//0 normal, 100 AFib
    GU8 uchAFibConfidence;//reserve
    GU16 usMaxSlope[4];//谷到峰的最大斜率
    GU16 usMaxSlopeTime[4];//谷到最大斜率的时间
    GU16 usRRI[4];
    GU16 usAC[4];
    GU16 usSystolicTime[4];
    GU16 usDiastolicTime[4];
    GS32 nA1[4];
    GS32 nA2[4];
} ST_BPF_RES;
#endif

typedef struct
{
    GU8 uchFrameID;
    GU8 led_drv[3];//physical led num//unused
    GU8 led_gain[3];
    GU8 led_current[3];
    GS32 sAccData[3];
    GS32 nGainAdjFlag[12];
    GS32 nRawdata[12];
    GS32 nCurAdjFlag[12];
    GS32 unSleepFlag;//unused
} ST_ALGO_CALCULATE_INFO;

typedef struct
{
    GU8 uchUpdateFlag;
    GU8 uchResultNum;
    GS32 snResult[16];
} ST_ALGO_RESULT;

/**
 * @brief channel map for function id
 */
typedef enum 
{ 
    GH3011_FUNC_OFFSET_HR = 1               , /**<1 HR FUNC_OFFSET */
    GH3011_FUNC_OFFSET_HRV = 2              , /**<2 HRV FUNC_OFFSET */
    GH3011_FUNC_OFFSET_NADT = 3             , /**<3 NADT FUNC_OFFSET */
    GH3011_FUNC_OFFSET_ADT_CONFIRM = 4      , /**<4 ADT_CONFIRM FUNC_OFFSET */
    GH3011_FUNC_OFFSET_SPO2 = 6             , /**<6 SPO2 FUNC_OFFSET */
    GH3011_FUNC_OFFSET_SOFT_ADT_GREEN = 9   , /**<9 SOFT ADT GREEN FUNC_OFFSET */
    GH3011_FUNC_OFFSET_SOFT_ADT_IR = 15     , /**<15 SOFT ADT IR FUNC_OFFSET */
    GH3011_FUNC_OFFSET_MAX
} EMFunctionID;

/* Hbd function return code definitions list. */
#define HBD_RET_OK                          (0)                /**< There is no error */
#define HBD_RET_GENERIC_ERROR               (-1)               /**< A generic error happens */
#define HBD_RET_PARAMETER_ERROR             (-2)               /**< Parameter error */
#define HBD_RET_COMM_NOT_REGISTERED_ERROR   (-3)               /**< I2c/Spi communication interface not registered error */
#define HBD_RET_COMM_ERROR                  (-4)               /**< I2c/Spi Communication error */
#define HBD_RET_RESOURCE_ERROR              (-5)               /**< Resource full or not available error */
#define HBD_RET_NO_INITED_ERROR             (-6)               /**< No inited error */
#define HBD_RET_LED_CONFIG_ALL_OFF_ERROR    (-7)               /**< Led config all off error */

#define HBD_INIT_CONFIG_DEFAULT_DEF(var) ST_HBD_INIT_CONFIG_TYPE var={\
                                                                      {HBD_FUNCTIONAL_STATE_ENABLE,\
                                                                      HBD_FUNCTIONAL_STATE_ENABLE,\
                                                                      HBD_FUNCTIONAL_STATE_ENABLE,\
                                                                      HBD_FUNCTIONAL_STATE_ENABLE,\
                                                                      HBD_FUNCTIONAL_STATE_ENABLE,\
                                                                      HBD_FUNCTIONAL_STATE_ENABLE},\
                                                                      {HBD_FUNCTIONAL_STATE_DISABLE,\
                                                                      HBD_LED_LOGIC_CHANNEL_MAP_PHY012, \
                                                                      HBD_LED_PD_SEL_INTERNAL, \
                                                                      HBD_LED_PD_SEL_INTERNAL,\
                                                                      0x19, 0x19, HBD_TIA_GAIN_3, HBD_TIA_GAIN_3, HBD_SAMPLE_TIME_128CYCLES}\
                                                                    }

/**
 * @fn     void HBD_AlgoRegisterHookFunc(GS32 (*pAlgoMemoryInitHookFunc)(void* mem_addr, GS32 size),
 *                               void (*pAlgoMemoryDeinitHookFunc)(void),
 *                               GS32 (*pAlgoVersionHookFunc)(EMFunctionID function_id,GU8 version[100]),
 *                               void (*pAlgoInitHookFunc)(EMFunctionID function_id,GU8 buffer_len,GS32 *algo_param_buffer),
 *                               void (*pAlgoCalculateHookFunc)(EMFunctionID function_id,ST_ALGO_CALCULATE_INFO *algo_calc_info,ST_ALGO_RESULT *algo_result),
 *                               void (*pAlgoDeinitHookFunc)(EMFunctionID function_id))
 *
 * @brief  algo register hook function callback
 *
 * @attention   None
 *
 * @param[in]   pAlgoMemoryInitHookFunc      pointer to algo memory init hook function
 * @param[in]   pAlgoMemoryDeinitHookFunc    pointer to algo memory deinit hook function
 * @param[in]   pAlgoVersionHookFunc         pointer to algo version function
 * @param[in]   pAlgoInitHookFunc            pointer to algo init hook function
 * @param[in]   pAlgoCalculateHookFunc       pointer to algo calculate hook function
 * @param[in]   pAlgoDeinitHookFunc          pointer to algo deinit hook function
 * @param[out]  None
 *
 * @return  None
 */
void HBD_AlgoRegisterHookFunc(GS32 (*pAlgoMemoryInitHookFunc)(GS32* error_code),
                                 void (*pAlgoMemoryDeinitHookFunc)(void),
                                 void (*pAlgoInitHookFunc)(EMFunctionID function_id,GU8 buffer_len,GS32 *algo_param_buffer),
                                 void (*pAlgoCalculateHookFunc)(EMFunctionID function_id,ST_ALGO_CALCULATE_INFO *algo_calc_info,ST_ALGO_RESULT *algo_result),
                                 void (*pAlgoDeinitHookFunc)(EMFunctionID function_id));

/****************************************************************
* Description: set i2c operation function
* Input:  uchI2cIdLowTwoBitsSelect: i2c low two bits addr selected, see EM_HBD_I2C_ID_SEL
          pI2cWriteFunc: i2c write function pointer
          pI2cReadFunc: i2c read function pointer
* Return: HBD_RET_PARAMETER_ERROR: function pointer parameter set null error,
          HBD_RET_OK: register success
******************************************************************/
GS8 HBD_SetI2cRW(GU8 uchI2cIdLowTwoBitsSelect,
                   GU8 (*pI2cWriteFunc)(GU8 uchDeviceId, const GU8 uchWriteBytesArr[], GU16 usWriteLen),  
                   GU8 (*pI2cReadFunc)(GU8 uchDeviceId, const GU8 uchCmddBytesArr[], GU16 usCmddLen, GU8 uchReadBytesArr[], GU16 usMaxReadLen));

/****************************************************************
* Description: set delay function callback
* Input:  pDelayUsFunc:delay function callback
* Return: None
******************************************************************/
void HBD_SetDelayUsCallback(void (*pDelayUsFunc)(GU16 usUsec));

/****************************************************************
* Description: load new GH30x config
* Input:    uchNewConfigArr: config array ,
            uchForceUpdateConfig: 1: need force update,0 :don't need, 
* Return: HBD_RET_OK:load config success, HBD_RET_COMM_ERROR: load config error,
******************************************************************/
GS8 HBD_LoadNewConfig(GU8 uchNewConfigArr[], GU8 uchForceUpdateConfig);

/****************************************************************
* Description: load new GH30x reg config array
* Input:    stRegConfigArr: reg config array ,
            usRegConfigLen: reg config array len, 
* Return: HBD_RET_OK:load config success, HBD_RET_COMM_ERROR: load config error,
******************************************************************/
GS8 HBD_LoadNewRegConfigArr(const ST_REGISTER stRegConfigArr[], GU16 usRegConfigLen);

/****************************************************************
* Description: Communication operation interface confirm.
* Output:  None
* Return: HBD_RET_OK=GH30x comunicate ok, 
          HBD_RET_COMM_ERROR=GH30x comunicate error,
          HBD_RET_COMM_NOT_REGISTERED_ERROR=GH30x i2c/spi interface not registered,
******************************************************************/
GS8 HBD_CommunicationInterfaceConfirm(void);

/****************************************************************
* Description: simple init hbd configure parameters
* Input:    stHbInitConfigParam: Init Config struct ,see ST_HBD_INIT_CONFIG_TYPE,
* Return: HBD_RET_OK=success, 
          HBD_RET_PARAMETER_ERROR=paramters error,
          HBD_RET_COMM_ERROR=GH30x comunicate error, 
          HBD_RET_COMM_NOT_REGISTERED_ERROR=GH30x i2c interface not registered
******************************************************************/
GS8 HBD_SimpleInit(ST_HBD_INIT_CONFIG_TYPE *stHbdInitConfigParam);

/****************************************************************
* Description: get chip id
* Input:  None
* Output: uchIdData: pointer to CHIP ID data buffer 
          uchIdLen: pointer to CHIP ID data buffer len
* Return: HBD_RET_OK=success, 
          HBD_RET_GENERIC_ERROR = chip state error,read reg fail
******************************************************************/
GS8 HBD_GetChipId(GU8 *uchIdData, GU8 *uchIdLen);

/****************************************************************
* Description: Control use HB algo or not 
* Input:  emState 
* Return: None
******************************************************************/
void HBD_EnableHBAlgo(EM_HBD_FUNCTIONAL_STATE emState);

/****************************************************************
* Description: start hbd
* Input:  None
* Return: HBD_RET_OK=success, 
          HBD_RET_NO_INITED_ERROR=not inited,
******************************************************************/
GS8 HBD_HbDetectStart(void);

/****************************************************************
* Description: start Wear state confirm.
* Input:  None, 
* Return: HBD_RET_OK=success, 
          HBD_RET_NO_INITED_ERROR=not inited,
          HBD_RET_LED_CONFIG_ALL_OFF_ERROR=all Led disable error,
          HBD_RET_GENERIC_ERROR=don't need check wear state, 
******************************************************************/
GS8 HBD_WearStateConfirmStart(void);

/****************************************************************
* Description: start in-ear ep wear detect, only use with hb detect
* Input:  None, 
* Return: HBD_RET_OK=success, 
          HBD_RET_NO_INITED_ERROR=not inited,
          HBD_RET_LED_CONFIG_ALL_OFF_ERROR=all Led disable error,
          HBD_RET_GENERIC_ERROR=wear detect function is disabled.
******************************************************************/
GS8 HBD_InearEpWearDetectStart(void);

/****************************************************************
* Description: recover in-ear ep detect, only use in autoled int 
* Input:  None, 
* Return: HBD_RET_OK=success, 
          HBD_RET_NO_INITED_ERROR=not inited,
          HBD_RET_LED_CONFIG_ALL_OFF_ERROR=all Led disable error,
          HBD_RET_GENERIC_ERROR=wear detect function is disabled.
******************************************************************/
GS8 HBD_InearEpDetectRecover (void);

/****************************************************************
* Description: start wear detect, only use with hb detect
* Input:  None, 
* Return: HBD_RET_OK=success, 
          HBD_RET_NO_INITED_ERROR=not inited,
          HBD_RET_LED_CONFIG_ALL_OFF_ERROR=all Led disable error,
          HBD_RET_GENERIC_ERROR=wear detect function is disabled.
******************************************************************/
GS8 HBD_SoftWearDetectStart (void);

/****************************************************************
* Description: recover detect, only use in autoled int 
* Input:  None, 
* Return: HBD_RET_OK=success, 
          HBD_RET_NO_INITED_ERROR=not inited,
          HBD_RET_LED_CONFIG_ALL_OFF_ERROR=all Led disable error,
          HBD_RET_GENERIC_ERROR=wear detect function is disabled.
******************************************************************/
GS8 HBD_SoftWearDetectRecover (void);

/****************************************************************
* Description: start hrv
* Input:  None
* Return: HBD_RET_OK=success, 
          HBD_RET_NO_INITED_ERROR=not inited,
******************************************************************/
GS8 HBD_HrvDetectStart(void);

#if (__HBD_BPF_ENABLE__)
/****************************************************************
* Description: Get BPF algorithm version
* Input:    None,
* Return: algorithm version
******************************************************************/
GU8 *HBD_GetBpfVersion(void);

/****************************************************************
* Description: Get AF algorithm version
* Input:    None,
* Return: algorithm version
******************************************************************/
GU8 *HBD_GetAFVersion(void);

/****************************************************************
* Description: start blood pressure feature detect
* Input:  None
* Return: HBD_RET_OK=success, 
          HBD_RET_LED_CONFIG_ALL_OFF_ERROR=all Led disable error,
          HBD_RET_NO_INITED_ERROR=not inited,
******************************************************************/
GS8 HBD_BpfDetectStart(void);
#endif

/****************************************************************
* Description: start hbd with hrv
* Input:  None
* Return: HBD_RET_OK=success, 
          HBD_RET_LED_CONFIG_ALL_OFF_ERROR=all Led disable error,
          HBD_RET_NO_INITED_ERROR=not inited,
******************************************************************/
GS8 HBD_HbWithHrvDetectStart (void);

/****************************************************************
* Description: stop hbd
* Input:  None
* Return: HBD_RET_OK=success, 
          HBD_RET_NO_INITED_ERROR=fail:don't init success 
******************************************************************/
GS8 HBD_Stop(void);

/****************************************************************
* Description: start adt wear detect
* Input:  None
* Return: HBD_RET_OK=success, 
          HBD_RET_NO_INITED_ERROR=fail:don't init success 
******************************************************************/
GS8 HBD_AdtWearDetectStart(void);

/****************************************************************
* Description: start adt shedding detect
* Input:  None
* Return: HBD_RET_OK=success, 
          HBD_RET_NO_INITED_ERROR=fail:don't init success 
******************************************************************/
GS8 HBD_AdtSheddingDetectStart (void);

/****************************************************************
* Description: start adt wear detect continuous
* Input:  None
* Return: HBD_RET_OK=success, 
          HBD_RET_NO_INITED_ERROR=fail:don't init success 
******************************************************************/
GS8 HBD_AdtWearContinuousDetectStart(void);

/****************************************************************
* Description: check adt has started
* Input:  None
* Return: 1= has started, 
          0= has not started
******************************************************************/
GU8 HBD_IsAdtWearDetectHasStarted(void);

/****************************************************************
* Description: get Int status 
* Input:  None
* Return: Status, see EM_INT_STATUS
******************************************************************/
GU8 HBD_GetIntStatus(void);

/****************************************************************
* Description: calculate hbd_value and wearing detect by fifo int.
* Input:  stGsAxisValue: gsensor data buffer
             usGsDataNum: gsensor data count
             emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output:   puchHbValue:ptr of heartbeat value output
            puchWearingState:ptr of wearing state output
            puchWearingQuality: ptr of wearing quality output
            puchVoiceBroadcast: ptr of voice broadcast output
            pusRRvalue: ptr of RR value output 
* Return: refresh flag, if heartbeat value have refresh that return 1.
******************************************************************/
GU8 HBD_HbCalculateByFifoInt(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, 
                            GU8 *puchHbValue, GU8 *puchWearingState, GU8 *puchWearingQuality, GU8 *puchVoiceBroadcast, GU16 *pusRRvalue);

/****************************************************************
* Description: calculate hbd_value and wearing detect by fifo int.
* Input:  stGsAxisValue: gsensor data buffer
             usGsDataNum: gsensor data count
             emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output:   puchHbValue:ptr of heartbeat value output
            puchWearingState:ptr of wearing state output
            puchWearingQuality: ptr of wearing quality output
            puchVoiceBroadcast: ptr of voice broadcast output
            pusRRvalue: ptr of RR value output 
            nRawdataOut: ptr of rawdata array output (ppg1, ppg2, gs-x, gs-y, gs-z, index)
            pusRawdataOutLen��ptr of rawdata len output
* Return: refresh flag, if heartbeat value have refresh that return 1.
******************************************************************/
GU8 HBD_HbCalculateByFifoIntDebugOutputData(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, 
                                            GU8 *puchHbValue, GU8 *puchWearingState, GU8 *puchWearingQuality, GU8 *puchVoiceBroadcast, GU16 *pusRRvalue, 
                                            GS32 nRawdataOut[][6], GU16 *pusRawdataOutLen);

/****************************************************************
* Description: calculate hbd_value and wearing detect by fifo int.
* Input:  stGsAxisValue: gsensor data buffer
             usGsDataNum: gsensor data count
             emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output:   puchHbValue:ptr of heartbeat value output
            puchAccuracyLevel: ptr of heartbeat accuracy level, max 100
            puchWearingState:ptr of wearing state output
            puchWearingQuality: ptr of wearing quality output
            puchVoiceBroadcast: ptr of voice broadcast output
            pusRRvalue: ptr of RR value output 
* Return: refresh flag, if heartbeat value have refresh that return 1.
******************************************************************/
GU8 HBD_HbCalculateWithLvlByFifoInt(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, GU8 *puchHbValue, GU8 *puchAccuracyLevel, 
                                    GU8 *puchWearingState, GU8 *puchWearingQuality, GU8 *puchVoiceBroadcast, GU16 *pusRRvalue);

/****************************************************************
* Description: calculate hbd_value and wearing detect by fifo int.
* Input:  stGsAxisValue: gsensor data buffer
             usGsDataNum: gsensor data count
             emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output:   puchHbValue:ptr of heartbeat value output
            puchAccuracyLevel: ptr of heartbeat accuracy level, max 100
            puchWearingState:ptr of wearing state output
            puchWearingQuality: ptr of wearing quality output
            puchVoiceBroadcast: ptr of voice broadcast output
            pusRRvalue: ptr of RR value output 
            nRawdataOut: ptr of rawdata array output (ppg1, ppg2, gs-x, gs-y, gs-z, index)
            pusRawdataOutLen��ptr of rawdata len output
* Return: refresh flag, if heartbeat value have refresh that return 1.
******************************************************************/
GS8 HBD_HbCalculateWithLvlByFifoIntDebugOutputData(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, 
                                            GU8 *puchHbValue, GU8 *puchAccuracyLevel, GU8 *puchWearingState, GU8 *puchWearingQuality, GU8 *puchVoiceBroadcast, GU16 *pusRRvalue, 
                                            GS32 nRawdataOut[][6], GU16 *pusRawdataOutLen);
                                            
/****************************************************************
* Description: calculate hbd_value and wearing detect by fifo int.
* Input:  stGsAxisValue: gsensor data buffer
             usGsDataNum: gsensor data count
             emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output:   *pstHbRes: ptr of hb calculate result ,seeST_HB_RES
            nRawdataOut: ptr of rawdata array output (ppg1, ppg2, gs-x, gs-y, gs-z, index)
            pusRawdataOutLen:ptr of rawdata len output
* Return: refresh flag, if heartbeat value have refresh that return 1.
******************************************************************/
GS8 HBD_HbCalculateByFifoIntEx(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, 
                                            GS32 nRawdataOut[][6], GU16 *pusRawdataOutLen, ST_HB_RES *pstHbRes);

/****************************************************************
* Description: calculate hbd_value and wearing detect by newdata int. must started with HBD_HbWithHrvDetectStart
* Input:  stGsAxisValue: gsensor data
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output:   puchHbValue:ptr of heartbeat value output
            puchWearingState:ptr of wearing state output
            puchWearingQuality: ptr of wearing quality output
            puchVoiceBroadcast: ptr of voice broadcast output
            pusRRvalue: ptr of RR value output 
            usRRvalueArr: ptr of RR value output 
            puchRRvalueCnt: ptr of RR value count output
            puchHrvConfidentLvl: ptr of RR value confident
* Return: refresh flag, 1:heartbeat refresh, 2:wearingstate refresh,3: heartbeat&wearingstate refresh.
******************************************************************/
GS8 HBD_HbWithHrvCalculateByNewdataInt(ST_GS_DATA_TYPE *stGsAxisValue, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, 
                    GU8 *puchHbValue, GU8 *puchWearingState, GU8 *puchWearingQuality, GU8 *puchVoiceBroadcast, 
                    GU16 usRRvalueArr[4], GU8 *puchRRvalueCnt, GU8 *puchHrvConfidentLvl);

/****************************************************************
* Description: calculate hbd_value and wearing detect by newdata int. must started with HBD_HbWithHrvDetectStart
* Input:  stGsAxisValue: gsensor data
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output:   pstHbRes:ptr of hb calculate result output,see ST_HB_RES
            pstHrvRes:ptr of hrv calculate result output,see ST_HRV_RES
* Return: refresh flag, 1:heartbeat refresh, 2:wearingstate refresh,3: heartbeat&wearingstate refresh.
******************************************************************/
GS8 HBD_HbWithHrvCalculateByNewdataIntEx(ST_GS_DATA_TYPE *stGsAxisValue, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, 
                                          ST_HB_RES *pstHbRes, ST_HRV_RES *pstHrvRes);

/****************************************************************
* Description: calculate hbd_value,wearing detect and hrv by fifo int. must started with HBD_HbWithHrvDetectStart
* Input:  stGsAxisValue: gsensor data buffer
             usGsDataNum: gsensor data count
             emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output:   puchHbValue:ptr of heartbeat value output
            puchAccuracyLevel: ptr of heartbeat accuracy level, max 100
            puchWearingState:ptr of wearing state output
            puchWearingQuality: ptr of wearing quality output
            puchVoiceBroadcast: ptr of voice broadcast output
            usRRvalueArr: ptr of RR value output 
            puchRRvalueCnt: ptr of RR value count output
            puchHrvConfidentLvl: ptr of RR value confident
            nRawdataOut: ptr of rawdata array output (ppg1, ppg2, gs-x, gs-y, gs-z, index)
            pusRawdataOutLen��ptr of rawdata len output
* Return: refresh flag, if heartbeat value have refresh that return 1.
******************************************************************/
GS8 HBD_HbWithHrvCalculateByFifoIntDbgData(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, 
                            GU8 *puchHbValue, GU8 *puchAccuracyLevel, GU8 *puchWearingState, GU8 *puchWearingQuality, GU8 *puchVoiceBroadcast, 
                            GU16 usRRvalueArr[4], GU8 *puchRRvalueCnt, GU8 *puchHrvConfidentLvl,
                            GS32 nRawdataOut[][6], GU16 *pusRawdataOutLen);
     
/****************************************************************
* Description: calculate hbd_value,wearing detect and hrv by fifo int. must started with HBD_HbWithHrvDetectStart
* Input:  stGsAxisValue: gsensor data buffer
             usGsDataNum: gsensor data count
             emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output:   *pstHbRes:ptr of hb calculate result,see ST_HB_RES
            *pstHrvRes:ptr of hrv calculate result,see ST_HRV_RES
            nRawdataOut: ptr of rawdata array output (ppg1, ppg2, gs-x, gs-y, gs-z, index)
            pusRawdataOutLen:ptr of rawdata len output
* Return: refresh flag, if heartbeat value have refresh that return 1.
******************************************************************/
GS8 HBD_HbWithHrvCalculateByFifoIntDbgDataEx(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, 
                            ST_HB_RES *pstHbRes, ST_HRV_RES *pstHrvRes, GS32 nRawdataOut[][6], GU16 *pusRawdataOutLen);

/****************************************************************
* Description: Wear state confirm.
* Output:   None,
* Return: wear state, 0: during wear state detect;
                      1: state output wear 
                      2: state output unwaer 
******************************************************************/
GU8 HBD_WearStateConfirm(void);

/****************************************************************
* Description: Wear state Detect.
* Output:   None,
* Return: wear state, 0: during wear state detect;
                      1: state output wear 
                      2: state output unwaer 
******************************************************************/
GU8 HBD_WearStateConfirmEx (void);

/****************************************************************
* Description: Wear state confirm by new int.
* Output:   None,
* Return: wear state, 0: during wear state detect;
                      1: state output wear 
                      2: state output unwaer 
******************************************************************/
GU8 HBD_WearStateConfirmByNewdataInt(void);

/****************************************************************
* Description: Wear state confirm by new int.
* Output:   None,
* Return: wear state, 0: during wear state detect;
                      1: state output wear 
                      2: state output unwaer 
******************************************************************/
GU8 HBD_WearStateConfirmByFifoInt (void);

/****************************************************************
* Description: enable wearing and setting direction array
* Input:    fDirectionArr: gsensor direction 
* Return: None
******************************************************************/
void HBD_EnableWearing(GF32 fDirectionArr[3]);

/****************************************************************
* Description: calculate hrv_value by newdata int.
* Input:  stGsAxisValue: gsensor data, if want get algo static state value, could set NULL.
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output: pusHrvRRValueArr:ptr of hrv RR value output array
* Return: refresh cnt, all count of RR value have refreshed.
******************************************************************/
GS8 HBD_HrvCalculateByNewdataInt(ST_GS_DATA_TYPE *stGsAxisValue, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, GU16 *pusHrvRRValueArr);

/****************************************************************
* Description: calculate hrv_value by fifo int.
* Input:  stGsAxisValue: gsensor data buffer, if want get algo static state value, could set NULL.
          usGsDataNum: gsensor data count, if want get algo static state value, could set less thah 100.
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output: pusHrvRRValueArr:ptr of hrv RR value output array
* Return: refresh cnt, all count of RR value have refreshed.
******************************************************************/
GS8 HBD_HrvCalculateByFifoInt(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, GU16 *pusHrvRRValueArr);

/****************************************************************
* Description: calculate hrv_value by fifo int.
* Input:  stGsAxisValue: gsensor data buffer, if want get algo static state value, could set NULL.
          usGsDataNum: gsensor data count, if want get algo static state value, could set less thah 100.
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output: pusHrvRRValueArr:ptr of hrv RR value output array
          nRawdataOut: ptr of rawdata array output (ppg1, ppg2, gs-x, gs-y, gs-z, index)
          pusRawdataOutLen��ptr of rawdata len output
* Return: refresh cnt, all count of RR value have refreshed.
******************************************************************/
GS8 HBD_HrvCalculateByFifoIntDbgRawdata(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, 
                                          GU16 *pusHrvRRValueArr, GS32 nRawdataOut[][6], GU16 *pusRawdataOutLen);

/****************************************************************
* Description: calculate hrv_value by newdata int.
* Input:  stGsAxisValue: gsensor data, if want get algo static state value, could set NULL.
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output: pusHrvRRValueArr:ptr of hrv RR value output array
          puchConfidentLvl:ptr of confident level, 0:no confidence, 1:confidence
* Return: refresh cnt, all count of RR value have refreshed.
******************************************************************/
GS8 HBD_HrvCalculateWithLvlByNewdataInt(ST_GS_DATA_TYPE *stGsAxisValue, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, GU16 *pusHrvRRValueArr, GU8 *puchConfidentLvl);

/****************************************************************
* Description: calculate hrv_value by fifo int.
* Input:  stGsAxisValue: gsensor data buffer, if want get algo static state value, could set NULL.
          usGsDataNum: gsensor data count, if want get algo static state value, could set less thah 100.
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output: pusHrvRRValueArr:ptr of hrv RR value output array
          puchConfidentLvl:ptr of confident level, 0:no confidence, 1:confidence
* Return: refresh cnt, all count of RR value have refreshed.
******************************************************************/
GS8 HBD_HrvCalculateWithLvlByFifoInt(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, GU16 *pusHrvRRValueArr, GU8 *puchConfidentLvl);

/****************************************************************
* Description: calculate hrv_value by fifo int.
* Input:  stGsAxisValue: gsensor data buffer, if want get algo static state value, could set NULL.
          usGsDataNum: gsensor data count, if want get algo static state value, could set less thah 100.
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output: pusHrvRRValueArr:ptr of hrv RR value output array
          puchConfidentLvl:ptr of confident level, 0:no confidence, 1:confidence
          nRawdataOut: ptr of rawdata array output (ppg1, ppg2, gs-x, gs-y, gs-z, index)
          pusRawdataOutLen��ptr of rawdata len output
* Return: refresh cnt, all count of RR value have refreshed.
******************************************************************/
GS8 HBD_HrvCalculateWithLvlByFifoIntDbgRawdata(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, 
                                       GU16 *pusHrvRRValueArr, GU8 *puchConfidentLvl, GS32 nRawdataOut[][6], GU16 *pusRawdataOutLen);
									   
/****************************************************************
* Description: calculate hrv_value by fifo int.
* Input:  stGsAxisValue: gsensor data buffer, if want get algo static state value, could set NULL.
          usGsDataNum: gsensor data count, if want get algo static state value, could set less thah 100.
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output: pstHrvRes:ptr of hrv result
          nRawdataOut: ptr of rawdata array output (ppg1, ppg2, gs-x, gs-y, gs-z, index)
          pusRawdataOutLen:ptr of rawdata len output
* Return: refresh cnt, all count of RR value have refreshed.
******************************************************************/
GS8 HBD_HrvCalculateByFifoIntEx(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, ST_HRV_RES *pstHrvRes, GS32 nRawdataOut[][6], GU16 *pusRawdataOutLen);

#if (__HBD_BPF_ENABLE__)
/****************************************************************
* Description: calculate blood pressure feature by newdata int.
* Input:  stGsAxisValue: gsensor data, if want get algo static state value, could set NULL.
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output: ST_BPF_RES *pstBPFRes  result output
* Return: refresh flag, if blood pressure value have refresh that return 1.
******************************************************************/
GU8 HBD_BpfCalculateByNewdataInt(ST_GS_DATA_TYPE *stGsAxisValue, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, 
                                  ST_BPF_RES *pstBPFRes);

/****************************************************************
* Description: calculate blood pressure feature by fifo int.
* Input:  stGsAxisValue: gsensor data buffer, if want get algo static state value, could set NULL.
          usGsDataNum: gsensor data count, if want get algo static state value, could set less thah 25. 
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output: ST_BPF_RES *pstBPFRes  result output
* Return: refresh flag, if blood pressure feature have refresh that return 1.
******************************************************************/
GU16 HBD_BpfCalculateByFifoIntDbg(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, 
                                ST_BPF_RES *pstBPFRes, GS32 nRawdataOut[][6], GU16 *pusRawdataOutLen);
#endif


/****************************************************************
* Description: calculate wear state by newdata int.
* Input:  stGsAxisValue: gsensor data, if want get algo static state value, could set NULL.
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output: puchWearState:ptr of wear state value output, 0:default, 1:wear, 2:unwear,
* Return: refresh flag, if wear state value have refresh that return 1.
******************************************************************/
GU8 HBD_InearEpWearDetectCalculateByNewdataInt(ST_GS_DATA_TYPE *stGsAxisValue, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, GU8 *puchWearState);

/****************************************************************
* Description: calculate wear state by fifo int.
* Input:  stGsAxisValue: gsensor data buffer, if want get algo static state value, could set NULL.
          usGsDataNum: gsensor data count, if want get algo static state value, could set less thah 25. 
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output: puchWearState:ptr of wear state value output, 0:default, 1:wear, 2:unwear,
* Return: refresh flag, if wear state value have refresh that return 1.
******************************************************************/
GU16 HBD_InearEpWearDetectCalculateByFifoInt(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, GU8 *puchWearState);

/****************************************************************
* Description: calculate wear state by fifo int.
* Input:  stGsAxisValue: gsensor data buffer, if want get algo static state value, could set NULL.
          usGsDataNum: gsensor data count, if want get algo static state value, could set less thah 25. 
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output: puchWearState:ptr of wear state value output, 0:default, 1:wear, 2:unwear,
          nRawdataOut: ptr of rawdata array output (ppg1, ppg2, gs-x, gs-y, gs-z, index)
          pusRawdataOutLen��ptr of rawdata len output
* Return: refresh flag, if wear state value have refresh that return 1.
******************************************************************/
GU16 HBD_InearEpWearDetectByFifoIntDbgRawdata(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, GU8 *puchWearState, GS32 nRawdataOut[][6], GU16 *pusRawdataOutLen);

/****************************************************************
* Description: get fifo count has read
* Input:  None,  
* Return: fifo count has read, 
******************************************************************/
GU8 HBD_GetFifoCntHasRead(void);

/****************************************************************
* Description: Reset chip
* Input:  None
* Return: HBD_RET_OK=success, 
          HBD_RET_COMM_NOT_REGISTERED_ERROR=GH30x i2c interface not registered
******************************************************************/
GS8 HBD_ChipReset(void);

/****************************************************************
* Description: set irq pluse width 
* Input:  uchIrqPluseWidth: irq width(us) setting ,
                            if set 0: fixed 1us and return HBD_RET_PARAMETER_ERROR
* Return: HBD_RET_OK=success, 
          HBD_RET_PARAMETER_ERROR=fail:parameter error
******************************************************************/
GS8 HBD_SetIrqPluseWidth(GU8 uchIrqPluseWidth);

/****************************************************************
* Description: change Hb config
* Input:  uchMode: 0:Hrv reconfig, else Hb reconfig
*         emFifoEnable: see EM_HBD_FUNCTIONAL_STATE
* Return: HBD_RET_OK=success, 
          HBD_RET_PARAMETER_ERROR=fail:parameter error,
******************************************************************/
GS8 HBD_FifoConfig(GU8 uchMode, EM_HBD_FUNCTIONAL_STATE emFifoEnable);

/****************************************************************
* Description: set irq pluse width 
* Input:  uchMode: 0:Hrv config, 
                   1:Hb config
                   2:hsm config
                   3:bpd config
                   4:pfa config
            usFifoCnt: Fifo thr setting (if enable two channel led,thr will auto * 2) 
* Return: HBD_RET_OK=success, 
          HBD_RET_PARAMETER_ERROR=fail:parameter error, usFifoCnt not allow 0, uchMode > 4
******************************************************************/
GS8 HBD_SetFifoThrCnt(GU8 uchMode, GU16 usFifoCnt);

/****************************************************************
* Description: config autoled channel
* Input:  stAutoLedChannelConfig: channal config
          uchChannel: channel index(1 or 2)
* Return: HBD_RET_OK=success, 
          HBD_RET_PARAMETER_ERROR=paramters error,
******************************************************************/
GS8 HBD_AutoLedChannelConfig(ST_AUTO_LED_CH_CONFIG stAutoLedChannelConfig, GU8 uchChannel);

/****************************************************************
* Description: config autoled bg thr 
* Input:  uchBgUpperThrConfig: bg upper thr config
          uchgLowerThrConfig: bg lower thr config 
* Return: HBD_RET_OK=success
******************************************************************/
GS8 HBD_AutoLedBgThrConfig(GU8 uchBgUpperThrConfig, GU8 uchgLowerThrConfig);

/****************************************************************
* Description: config autoled 
* Input:  stAutoLedCh1Config: channal 1 config
          stAutoLedCh2Config: channal 2 config
          uchBgUpperThrConfig: bg upper thr config
          uchgLowerThrConfig: bg lower thr config 
* Return: HBD_RET_OK=success, 
******************************************************************/
GS8 HBD_AutoLedConfig(ST_AUTO_LED_CH_CONFIG stAutoLedCh1Config, ST_AUTO_LED_CH_CONFIG stAutoLedCh2Config, 
                      GU8 uchBgUpperThrConfig, GU8 uchgLowerThrConfig);

/****************************************************************
* Description: config autoled gain start val
* Input:  emUseConfigValEnable: set HBD_FUNCTIONAL_STATE_ENABLE, use uchGainVal,
                                else use default val;
          uchGainVal: gain val, only 0-7 valid,  
* Return: HBD_RET_OK=success, 
******************************************************************/
GS8 HBD_AutoLedGainModeConfig(EM_HBD_FUNCTIONAL_STATE emUseConfigValEnable, GU8 uchGainVal);

/****************************************************************
* Description: config autoled current thr max val
* Input:  uchAutoledCurrentMaxThrVal: current max thr val;
* Return: HBD_RET_OK=success, 
******************************************************************/
GS8 HBD_AutoLedCurrentMaxThrConfig(GU8 uchAutoledCurrentMaxThrVal);

/****************************************************************
* Description: Get version
* Input:    None,
* Return: library Hbd ctrl version
******************************************************************/
GS8 * HBD_GetHbdVersion(void);

#if (ENABLE_SPO2_ABNORMAL_STATE)
/****************************************************************
* Description: Get SpO2_value good or not
* Input:    None,
* Return : 0-data good(signal good), 1-Detected motion, 2-PI too big, 3-data bad(signal bad)
******************************************************************/
GU8 HBD_GetSpo2AbnormalState (void);
#endif
/****************************************************************
* Description: start SpO2 and Hr
* Input:  None
* Return: HBD_RET_OK=success, 
          HBD_RET_LED_CONFIG_ALL_OFF_ERROR=all Led disable error,
          HBD_RET_NO_INITED_ERROR=not inited,
******************************************************************/
GS8 HBD_HrSpO2DetectStart (void);

/****************************************************************
* Description: start SpO2
* Input:  None
* Return: HBD_RET_OK=success, 
          HBD_RET_LED_CONFIG_ALL_OFF_ERROR=all Led disable error,
          HBD_RET_NO_INITED_ERROR=not inited,
******************************************************************/
GS8 HBD_SpO2DetectStart (void);

/****************************************************************
* Description: calculate SpO2_value by fifo int.
* Input:  stGsAxisValue: gsensor data buffer, if want get algo static state value, could set NULL.
          usGsDataNum: gsensor data count, if want get algo static state value, could set less thah 100.
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output:   pusSpo2Value : Spo2 Value
            puchSpo2ConfidentLvl: Spo2 Confident Level
            puchHbValue  : HB Value
            puchHbConfidentLvl: HB Confident Level
            pusHrvRRVal1 : HRV RRI value 1
            pusHrvRRVal2 : HRV RRI value 2
            pusHrvRRVal3 : HRV RRI value 3
            pusHrvRRVal4 : HRV RRI value 4
            puchHrvConfidentLvl: HRV Confident Level
            puchHrvcnt : HRV valid cnt
            pusSpo2RVal :spo2 R value
            pliSpo2InvalidFlag:InvalidFlag
            puchWearingState : Wearing State
* Return:  refresh flag, if spo2 value have refresh that return 1.
******************************************************************/
GU8 HBD_Spo2CalculateByFifoInt(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, GU8 *pusSpo2Value, GU8 *puchSpo2ConfidentLvl, GU8 *puchHbValue, GU8 *puchHbConfidentLvl,
                                                                           GU16 *pusHrvRRVal1,GU16 *pusHrvRRVal2,GU16 *pusHrvRRVal3,GU16 *pusHrvRRVal4, GU8 *puchHrvConfidentLvl, GU8 *puchHrvcnt,GU16 *pusSpo2RVal, GS32 *pliSpo2InvalidFlag, GU8 *puchWearingState);
/****************************************************************
* Description: calculate SpO2_value by newdata int.
* Input:  stGsAxisValue: gsensor data, if want get algo static state value, could set NULL.
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output:   pusSpo2Value : Spo2 Value
            puchSpo2ConfidentLvl: Spo2 Confident Level
            puchHbValue  : HB Value
            puchHbConfidentLvl: HB Confident Level
            pusHrvRRVal1 : HRV RRI value 1
            pusHrvRRVal2 : HRV RRI value 2
            pusHrvRRVal3 : HRV RRI value 3
            pusHrvRRVal4 : HRV RRI value 4
            puchHrvConfidentLvl: HRV Confident Level
            puchHrvcnt : HRV valid cnt
            pusSpo2RVal :spo2 R value
            pliSpo2InvalidFlag:InvalidFlag
            puchWearingState : Wearing State
* Return:  refresh flag, if spo2 value have refresh that return 1.
******************************************************************/
GS8 HBD_Spo2CalculateByNewdataInt(ST_GS_DATA_TYPE *stGsAxisValue, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity,  GU8 *pusSpo2Value, GU8 *puchSpo2ConfidentLvl, GU8 *puchHbValue, GU8 *puchHbConfidentLvl,
                                                                          GU16 *pusHrvRRVal1,GU16 *pusHrvRRVal2,GU16 *pusHrvRRVal3,GU16 *pusHrvRRVal4, GU8 *puchHrvConfidentLvl, GU8 *puchHrvcnt,GU16 *pusSpo2RVal, GS32 *pliSpo2InvalidFlag, GU8 *puchWearingState);

/****************************************************************
* Description: calculate SpO2_value by fifo int.
* Input:  stGsAxisValue: gsensor data buffer, if want get algo static state value, could set NULL.
          usGsDataNum: gsensor data count, if want get algo static state value, could set less thah 100.
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output:   pusSpo2Value : Spo2 Value
            puchSpo2ConfidentLvl: Spo2 Confident Level
            puchHbValue  : HB Value
            puchHbConfidentLvl: HB Confident Level
            pusHrvRRVal1 : HRV RRI value 1
            pusHrvRRVal2 : HRV RRI value 2
            pusHrvRRVal3 : HRV RRI value 3
            pusHrvRRVal4 : HRV RRI value 4
            puchHrvConfidentLvl: HRV Confident Level
            puchHrvcnt : HRV valid cnt
            pusSpo2RVal :spo2 R value
            pliSpo2InvalidFlag:InvalidFlag
            puchWearingState : Wearing State
* Return:  refresh flag, if spo2 value have refresh that return 1.
******************************************************************/
GU8 HBD_Spo2CalculateByFifoIntDbgRawdata(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, 
                                        GU8 *pusSpo2Value, GU8 *puchSpo2ConfidentLvl, GU8 *puchHbValue, GU8 *puchHbConfidentLvl,
                                     GU16 *pusHrvRRVal1,GU16 *pusHrvRRVal2,GU16 *pusHrvRRVal3,GU16 *pusHrvRRVal4, GU8 *puchHrvConfidentLvl, 
                                     GU8 *puchHrvcnt,GU16 *pusSpo2RVal, GS32 *pliSpo2InvalidFlag, GU8 *puchWearingState, GS32 nRawdataOut[][6], GU16 *pusRawdataOutLen);

/****************************************************************
* Description: calculate hbd_value and wearing detect by newdata int.
* Input:  stGsAxisValue: gsensor data
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output:   puchHbValue:ptr of heartbeat value output
            puchWearingState:ptr of wearing state output
            puchWearingQuality: ptr of wearing quality output
            puchVoiceBroadcast: ptr of voice broadcast output
            pusRRvalue: ptr of RR value output 
* Return: refresh flag, 1:heartbeat refresh, 2:wearingstate refresh,3: heartbeat&wearingstate refresh.
******************************************************************/
GS8 HBD_HbCalculateByNewdataIntDbg(ST_GS_DATA_TYPE *stGsAxisValue, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity,
                                   GU8 *puchHbValue, GU8 *puchWearingState, GU8 *puchWearingQuality, GU8 *puchVoiceBroadcast, GU16 *pusRRvalue);

/****************************************************************
* Description: calculate hbd_value and wearing detect by newdata int.
* Input:  stGsAxisValue: gsensor data
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output: *pstHbRes:ptr of hb calculate result,see ST_HB_RES
* Return: refresh flag, 1:heartbeat refresh, 2:wearingstate refresh,3: heartbeat&wearingstate refresh.
******************************************************************/
GS8 HBD_HbCalculateByNewdataIntDbgEx(ST_GS_DATA_TYPE *stGsAxisValue, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, ST_HB_RES *pstHbRes);

/****************************************************************
* Description: calculate SpO2_value by fifo int.
* Input:  stGsAxisValue: gsensor data buffer, if want get algo static state value, could set NULL.
          usGsDataNum: gsensor data count, if want get algo static state value, could set less thah 100.
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output:   pusSpo2Value : Spo2 Value
            puchSpo2ConfidentLvl: Spo2 Confident Level
            puchHbValue  : HB Value
            puchHbConfidentLvl: HB Confident Level
            pusHrvRRVal1 : HRV RRI value 1
            pusHrvRRVal2 : HRV RRI value 2
            pusHrvRRVal3 : HRV RRI value 3
            pusHrvRRVal4 : HRV RRI value 4
            puchHrvConfidentLvl: HRV Confident Level
            puchHrvcnt : HRV valid cnt
            pusSpo2RVal :spo2 R value
            pliSpo2InvalidFlag:InvalidFlag
            puchWearingState : Wearing State
            nRawdataOut : 
            pusRawdataOutLen : 
            puchValidLvl : 
* Return:  refresh flag, if spo2 value have refresh that return 1.
******************************************************************/
GS8 HBD_Spo2CalculateByFifoIntDbgRawdataInnerUse(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, 
                                                GU8 *pusSpo2Value, GU8 *puchSpo2ConfidentLvl, GU8 *puchHbValue, GU8 *puchHbConfidentLvl,
                                                GU16 *pusHrvRRVal1,GU16 *pusHrvRRVal2,GU16 *pusHrvRRVal3,GU16 *pusHrvRRVal4, GU8 *puchHrvConfidentLvl, 
                                                GU8 *puchHrvcnt,GU16 *pusSpo2RVal, GS32 *pliSpo2InvalidFlag, GU8 *puchWearingState, GS32 nRawdataOut[][6], GU16 *pusRawdataOutLen, GU8 *puchValidLvl);
                                                
/****************************************************************
* Description: calculate SpO2_value by fifo int.(new api)
* Input:    stGsAxisValue: gsensor data buffer, if want get algo static state value, could set NULL.
            usGsDataNum: gsensor data count, if want get algo static state value, could set less thah 100.
            emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
            pusRawdataOutLen : nRawdataOut length
* Output:   pstSpo2Res : spo2 result struct
            nRawdataOut : rawdata buf
            pusRawdataOutLen : RawdataOut real length
* Return:  refresh flag, if spo2 value have refresh that return 1.
******************************************************************/
GS8 HBD_Spo2CalculateByFifoIntEx(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, 
                                                GS32 nRawdataOut[][6], GU16 *pusRawdataOutLen, ST_SPO2_RES *pstSpo2Res);
/****************************************************************
* Description: calculate Hr_value and SpO2_value by fifo int.(new api)
* Input:    stGsAxisValue: gsensor data buffer, if want get algo static state value, could set NULL.
            usGsDataNum: gsensor data count, if want get algo static state value, could set less thah 100.
            emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
            pusRawdataOutLen : RawdataOut real length
* Output:   pstSpo2Res : spo2 result struct
            nRawdataOut : rawdata buf
            pusRawdataOutLen : RawdataOut real length
* Return:  refresh flag, if spo2 value have refresh that return 1.
******************************************************************/
GS8 HBD_HrSpo2CalculateByFifoIntEx(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity, 
                                                GS32 nRawdataOut[][6], GU16 *pusRawdataOutLen, ST_SPO2_RES *pstSpo2Res);

/****************************************************************
* Description: config adt confrim
* Input:  usAdtConfirmGsThrVal : gsensor confirm thr
          uchAdtConfirmGsCalcThrCntMax: gsensor confirm thr cnt max 
          uchAdtConfirmGsCalcThrCnt  : gsensor confirm thr cnt
* Return: None
******************************************************************/
void HBD_AdtConfirmConfig(GU16 usAdtConfirmGsThrVal, GU8 uchAdtConfirmGsCalcThrCntMax, GU8 uchAdtConfirmGsCalcThrCnt);

/****************************************************************
* Description: start adt confrim
* Input:  None
* Return: HBD_RET_OK=success, 
          HBD_RET_LED_CONFIG_ALL_OFF_ERROR=all Led disable error,
          HBD_RET_NO_INITED_ERROR=not inited,
******************************************************************/
GS8 HBD_AdtConfirmStart(void);

/****************************************************************
* Description: adt confirm detect by newdata int.
* Input:  stGsAxisValue: gsensor data
          emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output:  None
* Return: wear flag, 0x00: calc ing 0x11:wear, 0x12:unwear
******************************************************************/
GU8 HBD_AdtConfirmCalculateByNewdataInt(ST_GS_DATA_TYPE *stGsAxisValue, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity);

/****************************************************************
* Description: adt confirm detect by fifo int.
* Input:  stGsAxisValue: gsensor data buffer
             usGsDataNum: gsensor data count
             emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output:   None
* Return: wear flag, 0x00: calc ing 0x11:wear, 0x12:unwear
******************************************************************/
GU8 HBD_AdtConfirmCalculateByFifoInt(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity);

/****************************************************************
* Description: adt confirm detect by fifo int.
* Input:  stGsAxisValue: gsensor data buffer
             usGsDataNum: gsensor data count
             emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output:   nRawdataOut: ptr of rawdata array output (ppg1, ppg2, gs-x, gs-y, gs-z, index)
            pusRawdataOutLen��ptr of rawdata len output
* Return: wear flag, 0x00: calc ing 0x11:wear, 0x12:unwear
******************************************************************/
GU8 HBD_AdtConfirmCalculateByFifoIntDbgOutputData(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity,
                                                        GS32 nRawdataOut[][6], GU16 *pusRawdataOutLen);

/*****************************************************************************************
* Description: set LED current
* Input:  fLed0Current:LED0 current
          fLed1Current:LED1 current
* Return: HBD_RET_PARAMETER_ERROR: out of range,and set current to the closest limit value
          HBD_RET_GENERIC_ERROR: can't set current in this mode
          HBD_RET_OK: set current success
******************************************************************************************/
GS8 HBD_SetLedCurrent(GF32 fLed0Current, GF32 fLed1Current);

/****************************************************************
* Description: get LED current
* output:  pLed0Current:ptr of LED0 current
          pLed1Current:ptr of LED1 current
* Return: HBD_RET_GENERIC_ERROR: can't get current in this mode
          HBD_RET_OK: get current success
******************************************************************/
GS8 HBD_GetLedCurrrent(GF32 *pLed0Current, GF32 *pLed1Current);

/****************************************************************
* Description: Start HBD mode for get rawdata
* Input:  GU16 usSampleRate : sample rate
*         GU8 ucEnableFifo : 1 enable fifo, 0 disable fifo
*         GU8 ucFifoTHR : Fifo int generate threshold
* Return: 0 success, others fail
******************************************************************/
GS8 HBD_StartHBDOnly(GU16 usSampleRate, GU8 ucEnableFifo, GU16 ucFifoTHR);

/****************************************************************
* Description: Get rawdata in fifo int
* Input:  GU8 ucBufLen : rawdata array length(frames), if ucBufLen < *pucRealDataLen value, data will be lost
* Output: GS32 nRawdataOut: ptr of rawdata array output
* Return: 0 OK, 1 soft autoled error
******************************************************************/
GU8 HBD_GetRawdataByFifoInt(GU16 ucBufLen, GS32 nRawdataOut[][2], GU16 *pucRealDataLen);

/****************************************************************
* Description: Get rawdata in fifo int ,do not auto led
* Input:  GU8 ucBufLen : rawdata array length(frames), if ucBufLen < *pucRealDataLen value, data will be lost
* Output: GS32 nRawdataOut: ptr of rawdata array output
* Return: 0 OK
******************************************************************/
GU8 HBD_GetRawdataByFifoIntWithoutAutoled(GU16 ucBufLen, GS32 nRawdataOut[][2], GU16 *pucRealDataLen);

/****************************************************************
* Description: Get rawdata in new data int
* Output: GU32 *ppg1 
*         GU32 *ppg2
* Return: 0 OK, 1 soft autoled error
******************************************************************/
GU8 HBD_GetRawdataByNewDataInt(GU32 *ppg1, GU32 *ppg2);

/*****************************************************************************************
* Description: let chip enter resume mode when has read fifo ,so must be called after HBD_GetRawdataByFifoInt or HBD_GetRawdataByNewDataInt
* Output: 
* Return: 
*****************************************************************************************/
void HBD_GetRawdataHasDone(void);

/****************************************************************
* Description: Use IR ppg to do a simple check wear state by fifo int.
* Input:  stGsAxisValue: gsensor data buffer
             usGsDataNum: gsensor data count
             emGsensorSensitivity: G-sensor sensitivity (counts/g), see EM_HBD_GSENSOR_SENSITIVITY
* Output:   nRawdataOut: ptr of rawdata array output (ppg1, ppg2, gs-x, gs-y, gs-z, index)
            pusRawdataOutLen��ptr of rawdata len output
* Return: wear flag, 0x00: calc ing 0x11:wear, 0x12:unwear
******************************************************************/
GU8 HBD_IRSimpleWearCheckByFifoIntDbgOutputData(ST_GS_DATA_TYPE stGsAxisValue[], GU16 usGsDataNum, EM_HBD_GSENSOR_SENSITIVITY emGsensorSensitivity,
                                                    GS32 nRawdataOut[][6], GU16 *pusRawdataOutLen);



#endif /* __HBD_CTRL_H__ */

/********END OF FILE********* (C) COPYRIGHT 2018 .********/
