#include "vl53l0x.h"
#include <stdio.h>
#include "SysTick/bsp_SysTick.h"

/* 设备句柄和测量数据 */
static VL53L0X_Dev_t vl53l0x_dev;                     /* I2C 设备参数 */
static VL53L0X_RangingMeasurementData_t vl53l0x_data; /* 测距数据 */
static uint8_t is_initialized = 0;                   /* 初始化标志 */

static const mode_data DefaultMode = {
    (FixPoint1616_t)(0.25 * 65536),   /* sigmaLimit */
    (FixPoint1616_t)(18 * 65536),     /* signalLimit */
    33000,                            /* timingBudget (us) */
    14,                               /* preRangeVcselPeriod */
    10                                /* finalRangeVcselPeriod */
};

/**
 * @brief  打印错误信息
 * @param  Status VL53L0X 错误码
 */
static void print_pal_error(VL53L0X_Error Status)
{
    char buf[VL53L0X_MAX_STRING_LENGTH];
    VL53L0X_GetPalErrorString(Status, buf);
    printf("API Status: %i : %s\r\n", Status, buf);
}

/**
 * @brief  设置 VL53L0X 的 I2C 地址
 * @param  dev     设备结构体
 * @param  newaddr 新地址
 * @return 错误码
 */
static VL53L0X_Error vl53l0x_Addr_set(VL53L0X_Dev_t *dev, uint8_t newaddr)
{
    uint16_t Id;
    VL53L0X_Error Status = VL53L0X_ERROR_NONE;

    if (newaddr == dev->I2cDevAddr)
        return VL53L0X_ERROR_NONE;

    /* 确保 I2C 为标准模式 */
    Status = VL53L0X_WrByte(dev, 0x88, 0x00);
    if (Status != VL53L0X_ERROR_NONE) goto error;

    Status = VL53L0X_RdWord(dev, VL53L0X_REG_IDENTIFICATION_MODEL_ID, &Id);
    if (Status != VL53L0X_ERROR_NONE) goto error;

    if (Id == 0xEEAA) {
        Status = VL53L0X_SetDeviceAddress(dev, newaddr);
        if (Status != VL53L0X_ERROR_NONE) goto error;
        dev->I2cDevAddr = newaddr;

        /* 验证新地址是否工作 */
        Status = VL53L0X_RdWord(dev, VL53L0X_REG_IDENTIFICATION_MODEL_ID, &Id);
    }
error:
    if (Status != VL53L0X_ERROR_NONE) {
        print_pal_error(Status);
    }
    return Status;
}

/**
 * @brief  硬件复位 VL53L0X 并恢复 I2C 地址
 * @param  dev 设备结构体
 */
static void vl53l0x_reset(VL53L0X_Dev_t *dev)
{
    uint8_t orig_addr = dev->I2cDevAddr;
    LV_DISABLE(LV_XSH_PORT, LV_XSH_PIN);
    delay_ms(30);
    LV_ENABLE(LV_XSH_PORT, LV_XSH_PIN);
    delay_ms(30);
    dev->I2cDevAddr = 0x52;
    vl53l0x_Addr_set(dev, orig_addr);
    VL53L0X_DataInit(dev);
}

/**
 * @brief  VL53L0X 初始化（I2C、复位、基本参数）
 * @param  dev 设备结构体
 * @return 错误码
 */
static VL53L0X_Error vl53l0x_init(VL53L0X_Dev_t *dev)
{
    VL53L0X_Error Status = VL53L0X_ERROR_NONE;
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 初始化 XSHUT 引脚 */
    LV_XSH_PORT_CLK_ENABLE;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = LV_XSH_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LV_XSH_PORT, &GPIO_InitStructure);

    dev->I2cDevAddr = VL53L0X_Addr;   /* 默认 0x52 */
    dev->comms_type = 1;
    dev->comms_speed_khz = 400;

    VL53L0X_i2c_init();               /* 用户提供的 I2C 初始化 */

    LV_DISABLE(LV_XSH_PORT, LV_XSH_PIN);
    delay_ms(30);
    LV_ENABLE(LV_XSH_PORT, LV_XSH_PIN);
    delay_ms(30);

    /* 修改 I2C 地址（可选，此处改为 0x54） */
    Status = vl53l0x_Addr_set(dev, 0x54);
    if (Status != VL53L0X_ERROR_NONE) return Status;

    Status = VL53L0X_DataInit(dev);
    if (Status != VL53L0X_ERROR_NONE) return Status;
    delay_ms(2);

    /* 可选的设备信息读取（仅作调试） */
    //VL53L0X_DeviceInfo_t info;
    //VL53L0X_GetDeviceInfo(dev, &info);
    return Status;
}

/**
 * @brief  配置传感器为单次测距模式（默认参数，自动执行校准）
 * @param  dev 设备结构体
 * @return 错误码
 */
static VL53L0X_Error vl53l0x_set_default_mode(VL53L0X_Dev_t *dev)
{
    VL53L0X_Error status;
    uint8_t VhvSettings, PhaseCal;
    uint32_t refSpadCount;
    uint8_t isApertureSpads;

    /* 复位并执行静态初始化 */
    vl53l0x_reset(dev);
    status = VL53L0X_StaticInit(dev);
    if (status != VL53L0X_ERROR_NONE) goto error;

    /* 执行参考校准（动态） */
    status = VL53L0X_PerformRefCalibration(dev, &VhvSettings, &PhaseCal);
    if (status != VL53L0X_ERROR_NONE) goto error;
    delay_ms(2);

    /* 执行参考 SPAD 管理 */
    status = VL53L0X_PerformRefSpadManagement(dev, &refSpadCount, &isApertureSpads);
    if (status != VL53L0X_ERROR_NONE) goto error;
    delay_ms(2);

    /* 设置设备模式为单次测距 */
    status = VL53L0X_SetDeviceMode(dev, VL53L0X_DEVICEMODE_SINGLE_RANGING);
    if (status != VL53L0X_ERROR_NONE) goto error;
    delay_ms(2);

    /* 使能范围检查 */
    status = VL53L0X_SetLimitCheckEnable(dev, VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE, 1);
    if (status != VL53L0X_ERROR_NONE) goto error;
    status = VL53L0X_SetLimitCheckEnable(dev, VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, 1);
    if (status != VL53L0X_ERROR_NONE) goto error;

    /* 设置检查阈值（默认模式） */
    status = VL53L0X_SetLimitCheckValue(dev, VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE, DefaultMode.sigmaLimit);
    if (status != VL53L0X_ERROR_NONE) goto error;
    status = VL53L0X_SetLimitCheckValue(dev, VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, DefaultMode.signalLimit);
    if (status != VL53L0X_ERROR_NONE) goto error;

    /* 设置测距时间预算 */
    status = VL53L0X_SetMeasurementTimingBudgetMicroSeconds(dev, DefaultMode.timingBudget);
    if (status != VL53L0X_ERROR_NONE) goto error;

    /* 设置 VCSEL 脉冲周期 */
    status = VL53L0X_SetVcselPulsePeriod(dev, VL53L0X_VCSEL_PERIOD_PRE_RANGE, DefaultMode.preRangeVcselPeriod);
    if (status != VL53L0X_ERROR_NONE) goto error;
    status = VL53L0X_SetVcselPulsePeriod(dev, VL53L0X_VCSEL_PERIOD_FINAL_RANGE, DefaultMode.finalRangeVcselPeriod);

error:
    if (status != VL53L0X_ERROR_NONE) {
        print_pal_error(status);
    }
    return status;
}

/**
 * @brief  执行单次测距，返回距离值（毫米）
 * @param  dev 设备结构体
 * @param  pdata 测量数据输出
 * @return 错误码
 */
static VL53L0X_Error vl53l0x_start_single_test(VL53L0X_Dev_t *dev,
                                               VL53L0X_RangingMeasurementData_t *pdata)
{
    VL53L0X_Error status = VL53L0X_PerformSingleRangingMeasurement(dev, pdata);
    if (status != VL53L0X_ERROR_NONE) {
        print_pal_error(status);
    }
    return status;
}

/**
 * @brief  对外接口：获取一次测距值（毫米）
 * @return 距离值（毫米），若出错则返回 0
 */
uint16_t vl53l0x_Get_Distance(void)
{
    /* 首次调用时执行完整初始化 */
	VL53L0X_Error status;
    if (!is_initialized) {
        VL53L0X_Error err = vl53l0x_init(&vl53l0x_dev);
        if (err != VL53L0X_ERROR_NONE) {
            printf("VL53L0X 初始化失败\r\n");
            return 0;
        }
        err = vl53l0x_set_default_mode(&vl53l0x_dev);
        if (err != VL53L0X_ERROR_NONE) {
            printf("模式配置失败\r\n");
            return 0;
        }
        is_initialized = 1;
        printf("VL53L0X 初始化成功\r\n");
    }

    /* 执行单次测距 */
    status = vl53l0x_start_single_test(&vl53l0x_dev, &vl53l0x_data);
    if (status != VL53L0X_ERROR_NONE) {
        return 0;   /* 测距失败*/
    }

    /* 返回距离（毫米） */
    return vl53l0x_data.RangeMilliMeter;
}
uint16_t Distance_Update(uint8_t frequency)
{
    static uint16_t cnt = 0;
    static uint16_t distance = 0;

    cnt++;

    if (cnt >= frequency)
    {
        cnt = 0;
        distance = vl53l0x_Get_Distance();
    }

    return distance;
}


