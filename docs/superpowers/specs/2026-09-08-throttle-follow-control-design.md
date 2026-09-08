# 油门与定距跟随控制设计

**状态：** 已完成设计确认，下一阶段可编写实施计划。定距跟随状态机需要实现，但在当前固件中保持关闭，且暂不为它定义用户可操作的进入或退出方式。

## 目标

为 STM32F103C8 智能车增加独立、可复用的整车控制层。该控制层支持两种单踏板油门方式和基于 PID 的定距跟随状态机，同时保留已有的底层驱动，并在车辆前进时执行 150 mm 的自动紧急制动限制。

## 范围

本设计包括：

- 踏板开度直接映射 PWM。
- 踏板开度映射目标转速，再用速度 PID 输出 PWM。
- 目标距离为 250 mm 的定距跟随状态机和距离 PID。
- 车辆前进且障碍物距离小于或等于 150 mm 时自动紧急制动，倒车仍可用于远离障碍物。
- 为将来的 FG 脉冲测速和将来的跟随模式触发方式预留稳定接口。

本设计不包括定距跟随的进入/退出触发方式，不增加编码器 GPIO 配置，也不确定最终 PID 参数。默认构建中定距跟随始终关闭。

## 已知条件与限制

- 工程采用 Keil MDK5、STM32F103C8 和 STM32F10x 标准外设库。
- `Doc/readme.txt` 规定硬件引脚连接已固定，不可随意修改。
- `User/main.c` 通过 `__RF24L01_TX_TEST__` 与 `_MAIN_ECU_` 选择方向盘端、车端主 ECU、测距 ECU 三种固件角色。
- 方向盘端的无线帧长为 8 字节，包含方向盘 ADC 高低字节、踏板值、D/R/N 档位、左右灯信号，以及 `0x01`、`0x02`、`0x03` 三个标志字节。
- `User/motor/motor.c` 已提供两个电机的独立 PWM 输出函数，`Motor_SetCW()` 仍等待实现。
- `User/turn`、`User/ws2812b`、`User/oled` 与 `User/vl53l0/demo` 已分别提供转向、灯光、显示和 `Distance_Update()` 测距接口。
- 第 17 页电机资料给出 FG 信号每电机转 18 个脉冲。FG 实际接线、减速比和输出轴转速标定值尚未确认。

## 架构

新增 `User/control/`，将控制逻辑与底层硬件驱动分开：

1. `pid.c/.h`：提供带限幅的离散 PID，维护积分项、微分项、输出范围和复位状态。调用方以毫秒传入实际控制周期，使控制周期将来可以从初始固定周期改为定时器调度，而不改变 PID 的使用方式。
2. `vehicle_control.c/.h`：将方向盘端输入和车辆反馈转换为 `DriveCommand`。它负责选择油门模式、执行单踏板方向规则、在反馈有效时运行速度 PID、在跟随已启用时运行距离 PID，并统一执行自动紧急制动判断。
3. `speed_feedback.c/.h`：定义轮速反馈边界。第一版返回“反馈无效”，因为暂未确认 FG 引脚；以后接入 FG 计数后，只需替换该模块，油门和跟随控制逻辑无需重写。

电机、舵机、OLED、WS2812、无线和 VL53L0X 驱动仍保持独立。`main.c` 只负责硬件初始化、无线帧校验、采集传感器快照，以及将快照传给 `VehicleControl_Update()`。

## 两种油门模式

### 模式 1：直接 PWM

踏板字节 `0..255` 线性映射为 `0..100` 的 PWM 占空比。D 档输出前进，R 档输出后退，N 档强制输出零占空比。该模式不依赖轮速反馈，作为默认模式，用于完成基础功能和早期安全调试。

### 模式 2：目标转速 PID

踏板字节映射为可配置的目标输出轴转速 RPM。D 档给正目标转速，R 档给负目标转速，N 档给零目标转速。速度 PID 比较目标转速和实际转速，输出受限的 PWM 占空比。

若轮速反馈无效，模式 2 必须输出停车命令，不能把“没有反馈”错误地当作“当前转速为零”。公开接口中模式数值固定为 `1` 和 `2`，默认编译配置为模式 1。

## 定距跟随状态机

定距跟随代码会存在于工程中，但默认关闭。保留 `VehicleControl_SetFollowEnabled()` 接口，当前不由无线帧、开关或按键调用，等待后续确定进入和退出方式。

当未来明确调用该接口启用跟随时：

- 跟随仅允许前进。若遥控输入为 R 档，跟随控制输出停车命令；关闭跟随后，遥控器仍可以正常执行倒车。
- 距离 PID 以 250 mm 为设定值，输出不小于零的目标转速，并限制在可配置的跟随最高转速以内。
- 距离 PID 的输出进入速度 PID，再由速度 PID 输出 PWM。这构成“距离到目标速度到 PWM”的串级控制，执行器限幅集中在一处。
- 距离有效且小于或等于 150 mm 时，强制输出零前进扭矩。
- 跟随模式中测距无效时，禁止输出前进命令。

状态机必须有明确的复位规则：关闭跟随时清空距离 PID 的积分和微分历史；再次启用后先输出停车命令，只接受新的有效测距数据。

## 公开接口

```c
typedef enum {
    THROTTLE_DIRECT_PWM = 1,
    THROTTLE_TARGET_SPEED_PID = 2
} ThrottleMode;

typedef enum {
    VEHICLE_CONTROL_MANUAL = 0,
    VEHICLE_CONTROL_FOLLOW = 1
} VehicleControlState;

typedef struct {
    uint16_t wheel_adc;
    uint8_t pedal;
    uint8_t dnr;
    uint8_t left_right;
} VehicleInput;

typedef struct {
    int16_t left_rpm;
    int16_t right_rpm;
    uint8_t speed_valid;
    uint16_t distance_mm;
    uint8_t distance_valid;
} VehicleFeedback;

typedef struct {
    uint8_t direction;
    uint8_t left_duty;
    uint8_t right_duty;
    uint8_t emergency_stop;
} DriveCommand;

void VehicleControl_Init(void);
void VehicleControl_SetThrottleMode(ThrottleMode mode);
ThrottleMode VehicleControl_GetThrottleMode(void);
void VehicleControl_SetFollowEnabled(uint8_t enabled);
VehicleControlState VehicleControl_GetState(void);
void VehicleControl_Update(const VehicleInput *input,
                           const VehicleFeedback *feedback,
                           uint32_t dt_ms,
                           DriveCommand *command);

void PID_Init(PID_Controller *pid, float kp, float ki, float kd,
              float output_min, float output_max);
void PID_Reset(PID_Controller *pid);
float PID_Update(PID_Controller *pid, float setpoint,
                 float measurement, float dt_s);
```

档位和左右灯信号会在 `main.c` 边界复用或转换已有 `switch.h` 的枚举，不在控制模块重复定义 GPIO 或硬件引脚。

## 配置项

控制常量集中放在 `vehicle_control.h` 或专用配置头文件中：

- `CONTROL_DEFAULT_THROTTLE_MODE = 1`
- `CONTROL_AEB_STOP_DISTANCE_MM = 150`
- `CONTROL_FOLLOW_DISTANCE_MM = 250`
- `CONTROL_FG_PULSES_PER_REV = 18`

最大目标 RPM 和 PID 增益必须作为明确的标定参数，不得从电机型号或工程图尺寸中猜测。所有 PWM 输出限制为现有驱动允许的 `0..100`。负目标转速通过“方向字段加正占空比”表示，避免无符号 PWM 变量接收负数。

## 主循环接入方式

主 ECU 最终按以下顺序工作：

1. 在 `Motor_Init()`、`Turn_Init()`、`OLED_Init()`、`WS2812_Init()` 后初始化控制模块。
2. 校验 NRF24L01 的 8 字节帧，再构造 `VehicleInput`。
3. 测距 ECU 通信链路完成后，读取最新的距离数据。
4. 从反馈模块取得轮速。第一版将其标记为无效。
5. 调用 `VehicleControl_Update()`，传入当前控制周期。
6. 使用返回的方向和 PWM 分别调用 `Motor_SetCW()`、`Motor1_SetDuty()`、`Motor2_SetDuty()`；转向、灯光和 OLED 显示仍独立更新。

初始接入时保持跟随关闭并使用直接 PWM，因此可以先完成基础行驶功能，再启用依赖编码器的模式 2 或跟随功能。

## 安全与异常处理

- 无线帧校验失败时忽略该帧，不得用错误数据更新控制命令。
- N 档或零踏板必须输出零占空比。
- 车辆前进且有效距离小于或等于 150 mm 时，强制输出零占空比，并通过 `emergency_stop` 供 OLED 或调试信息显示。
- 手动模式下倒车必须仍可用，确保车辆可以离开障碍物。
- 模式 2 轮速反馈无效时停车，不能继续盲目加速。
- 跟随模式测距无效时停车，不能继续前进。
- 控制器禁用、经过 N 档改变方向时，复位 PID 积分和微分历史；积分项也需限幅，防止积分饱和。

## 验证方式

硬件测试前，为纯计算逻辑增加主机侧测试，至少覆盖：

- 踏板映射的最小值、最大值和中间值。
- D/R/N 对应的方向选择。
- 模式 1 的占空比输出。
- 模式 2 的输出限幅和反馈无效停车。
- 距离在 250 mm 附近时跟随 PID 的输出方向。
- 距离为 150 mm 时的 AEB，以及手动倒车逃离障碍物。
- 跟随启用和关闭时 PID 复位的行为。

上车时先架空后轮，依次验证 N 档、前进、后退、舵机归中、150 mm 内紧急停车和倒车离开障碍物。只有这些测试通过后，才可以在受控直线场地手动启用定距跟随进行试验。
