# 三 ECU 智能车控制系统实施计划

> **给代理开发者：** 必须使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans`，按任务逐项执行。每一步都使用复选框跟踪；完成一个任务后先运行该任务的验证，再提交独立的小提交。

**目标：** 在现有 STM32F103C8 Keil 工程中实现三块 ECU 的明确算力分配、两种单踏板油门、测距 ECU 的定距跟随距离 PID、主 ECU 的速度 PID 与 150 mm AEB 安全仲裁，同时保持跟随模式默认关闭。

**架构：** 方向盘 ECU 只采集输入并通过 NRF24L01 发送；测距 ECU 负责 VL53L0X 测量、滤波和 250 mm 距离 PID，并通过 UART 发送建议目标转速；车端主 ECU 接收两条链路，运行速度 PID，最终控制电机、舵机、灯光和 OLED，所有前进安全限制由主 ECU 决定。

**技术栈：** STM32F103C8、Keil MDK5、STM32F10x Standard Peripheral Library、C89 风格嵌入式 C、NRF24L01、VL53L0X、USART1、TIM1/TIM4、WS2812B、OLED。

---

## 文件结构与职责

实现前固定以下文件边界，避免把三 ECU 逻辑继续堆入 `main.c`：

- 创建：`User/protocol/vehicle_protocol.h/.c`，三 ECU 共用的帧常量、CRC8、方向盘帧和测距 UART 帧编解码。
- 创建：`User/control/pid.h/.c`，无硬件依赖的带限幅离散 PID。
- 创建：`User/control/vehicle_control.h/.c`，主 ECU 的两种油门、跟随状态、AEB 和最终 `DriveCommand` 仲裁。
- 创建：`User/control/speed_feedback.h/.c`，FG 反馈抽象；在 FG 引脚确认前返回无效反馈。
- 创建：`User/ranging/range_filter.h/.c`，测距 ECU 的有效性检查和一阶滤波。
- 创建：`User/ranging/follow_control.h/.c`，250 mm 距离 PID，输出建议目标 RPM；默认由配置关闭。
- 创建：`User/ranging/range_uart.h/.c`，测距 ECU 的固定长度 UART 二进制发送。
- 修改：`User/main.c`，按宏选择三种 ECU 固件，接入非阻塞任务、协议解析和各模块输出。
- 修改：`User/main.h`，保留 ECU 角色宏，增加默认油门模式、组号和跟随关闭配置。
- 修改：`User/motor/motor.c`，实现 `Motor_SetCW()`，统一停止、正转、反转方向输出。
- 修改：`User/usart/bsp_usart.h/.c` 与 `User/stm32f10x_it.c`，增加 USART1 接收环形缓冲区和发送固定帧接口。
- 修改：`User/si24r1/drv_periph/inc/drv_RF24L01.h` 与 `src/drv_RF24L01.c`，增加非阻塞接收函数。
- 修改：`Project/RVMDK（uv5）/Fire_F103C8.uvprojx`，增加新目录的头文件搜索路径和源文件到 USER 组。
- 创建：`tests/control/test_control.c`，主机侧纯 C 单元测试；测试不加入 Keil USER 组。
- 修改：`README.md`，补充三 ECU 烧录、宏选择、地址和测试说明。

## 任务 1：建立三 ECU 共用协议

**文件：**

- 创建：`User/protocol/vehicle_protocol.h`
- 创建：`User/protocol/vehicle_protocol.c`
- 修改：`User/main.c`
- 修改：`User/main.h`
- 修改：`Project/RVMDK（uv5）/Fire_F103C8.uvprojx`
- 测试：`tests/control/test_control.c`

- [ ] **步骤 1：定义协议常量和数据结构**

在 `vehicle_protocol.h` 中写入固定定义：

```c
#define WHEEL_FRAME_LEN 8u
#define RANGE_FRAME_LEN 9u
#define RANGE_FRAME_HEAD0 0xA5u
#define RANGE_FRAME_HEAD1 0x5Au
#define CONTROL_AEB_STOP_DISTANCE_MM 150u
#define CONTROL_FOLLOW_DISTANCE_MM 250u
#define CONTROL_FG_PULSES_PER_REV 18u
#define RADIO_TIMEOUT_MS 100u
#define RANGE_UART_TIMEOUT_MS 120u

typedef enum {
    VEHICLE_DNR_NEUTRAL = 0,
    VEHICLE_DNR_FORWARD = 1,
    VEHICLE_DNR_REVERSE = 2
} VehicleDnr;

typedef struct {
    uint16_t wheel_adc;
    uint8_t pedal;
    uint8_t dnr;
    uint8_t left_right;
} WheelCommandFrame;

typedef struct {
    uint8_t sequence;
    uint16_t distance_mm;
    int16_t target_rpm;
    uint8_t status;
} RangeStatusFrame;

uint8_t VehicleProtocol_Crc8(const uint8_t *data, uint8_t length);
void VehicleProtocol_EncodeWheel(const WheelCommandFrame *frame, uint8_t *raw);
uint8_t VehicleProtocol_DecodeWheel(const uint8_t *raw, WheelCommandFrame *frame);
uint8_t VehicleProtocol_EncodeRange(const RangeStatusFrame *frame, uint8_t *raw);
uint8_t VehicleProtocol_DecodeRange(const uint8_t *raw, RangeStatusFrame *frame);
```

使用现有 D/R/N 数值约定，避免在 `switch.h` 和协议层之间产生转换错误。`target_rpm` 使用有符号 16 位整数，PWM 输出仍使用无符号占空比。

- [ ] **步骤 2：实现 CRC8 与帧编解码**

CRC8 使用多项式 `0x07`，初值为 `0x00`，计算范围为帧头之后至 CRC 前的全部字节。方向盘帧只接受 `raw[0]==0x01`、`raw[4]==0x02`、`raw[6]==0x03`；不满足时返回 0。测距帧严格检查两个帧头、长度和 CRC。

- [ ] **步骤 3：添加 Keil 文件与头文件搜索路径**

在 `.uvprojx` 的 USER 组加入 `vehicle_protocol.c`，并将 `..\..\User\protocol` 加入 `IncludePath`。确认 XML 中的 `FilePath` 使用工程已有的 `..\..\User\...` 相对格式。

- [ ] **步骤 4：写协议失败测试并运行**

在 `tests/control/test_control.c` 中先加入：合法方向盘帧通过、任意一个标志位错误失败、合法测距帧通过、修改任意数据后 CRC 失败。主机测试命令为：

```powershell
cc -std=c89 -Wall -Wextra -IUser\protocol tests\control\test_control.c User\protocol\vehicle_protocol.c -o tests\control\test_control.exe
tests\control\test_control.exe
```

期望输出 `protocol tests passed`，失败时进程返回非零值。

- [ ] **步骤 5：提交协议基础**

```powershell
git add User/protocol User/main.c User/main.h Project/RVMDK（uv5）/Fire_F103C8.uvprojx tests/control/test_control.c
git commit -m "feat: add three-ecu protocol"
```

## 任务 2：方向盘 ECU 输入与无线发送

**文件：**

- 修改：`User/main.c`
- 修改：`User/main.h`
- 修改：`User/switch/switch.c`（仅在发现输入初始化缺陷时修改）
- 修改：`User/si24r1/drv_periph/inc/drv_RF24L01.h`

- [ ] **步骤 1：补齐方向盘输入初始化**

方向盘分支在读取 `Get_DNR()` 和 `Get_LeftRight()` 前调用 `Switch_Init()`。保持现有 ADC 引脚：方向盘 ADC 使用 `PA2`，踏板 ADC 使用 `PA1`。不要修改 `Doc/readme.txt` 规定的硬件引脚。

- [ ] **步骤 2：保留 5 ms 发送周期并改用协议编码**

发送循环继续每 5 ms 采样一次，将 `Wheel_GetValue()`、`Pedal_GetValue()`、`Get_DNR()`、`Get_LeftRight()` 填入 8 字节帧；发送前使用协议函数构造标志字节。保留 NRF24L01 地址由 `INIT_ADDR` 宏统一配置，方向盘端与主 ECU 必须使用相同地址。

- [ ] **步骤 3：增加输入范围安全规则**

踏板值天然限制为 `0..255`；无线发送失败不改变下一帧输入。不要在方向盘 ECU 运行 PID 或测距。

- [ ] **步骤 4：用静态检查验证方向盘分支**

确认 `__RF24L01_TX_TEST__` 定义时只引用 `wheel.h`、`pedal.h`、`switch.h` 和 NRF24L01 发送 API；定义 `_MAIN_ECU_` 时不编译方向盘 ADC 初始化调用。使用 Keil 编译验证，期望无未定义符号。

- [ ] **步骤 5：提交方向盘 ECU**

```powershell
git add User/main.c User/main.h User/si24r1/drv_periph/inc/drv_RF24L01.h
git commit -m "feat: complete steering ecu input path"
```

## 任务 3：实现电机方向和通用 PID

**文件：**

- 修改：`User/motor/motor.c`
- 修改：`User/motor/motor.h`
- 创建：`User/control/pid.h`
- 创建：`User/control/pid.c`
- 测试：`tests/control/test_control.c`
- 修改：`Project/RVMDK（uv5）/Fire_F103C8.uvprojx`

- [ ] **步骤 1：先写电机方向失败测试**

在硬件抽象之外定义方向值：`0=停止，1=前进，2=后退`。测试只验证控制层生成的方向，不直接访问 GPIO。为 `Motor_SetCW()` 写注释说明实际 PB4/PB5 电平映射必须根据电机驱动板实测确认。

- [ ] **步骤 2：实现 `Motor_SetCW()`**

实现互锁顺序：先将两个 PWM 比较值设为 0，再改变 PB4/PB5 方向电平，最后由主循环重新写入 PWM。状态 0 清零 PB4、PB5；状态 1 和 2 使用互补电平。若实测正反方向相反，只允许在该函数内交换状态映射。

- [ ] **步骤 3：定义 PID 结构和实现**

`pid.h` 使用以下结构和接口：

```c
typedef struct {
    float kp;
    float ki;
    float kd;
    float integral;
    float previous_error;
    float output_min;
    float output_max;
    uint8_t initialized;
} PID_Controller;

void PID_Init(PID_Controller *pid, float kp, float ki, float kd,
              float output_min, float output_max);
void PID_Reset(PID_Controller *pid);
float PID_Update(PID_Controller *pid, float setpoint,
                 float measurement, float dt_s);
```

积分项按 `integral += error * dt_s` 更新并限制在输出范围；`dt_s<=0` 时只返回限幅后的比例项，不更新积分。第一次调用只建立微分历史，避免上电微分冲击。

- [ ] **步骤 4：运行 PID 单元测试**

测试比例方向、输出限幅、积分复位、零周期和微分历史。使用任务 1 的主机编译命令追加 `User\control\pid.c`，期望输出 `pid tests passed`。

- [ ] **步骤 5：加入 Keil 并提交**

将 `pid.c` 加入 USER 组、`..\..\User\control` 加入头文件路径，执行 Keil Build Target 0，确认无错误后提交：

```powershell
git add User/motor User/control/pid.* Project/RVMDK（uv5）/Fire_F103C8.uvprojx tests/control/test_control.c
git commit -m "feat: add motor direction and pid core"
```

## 任务 4：测距 ECU 的滤波、跟随 PID 和 UART 帧

**文件：**

- 创建：`User/ranging/range_filter.h/.c`
- 创建：`User/ranging/follow_control.h/.c`
- 创建：`User/ranging/range_uart.h/.c`
- 修改：`User/main.c`
- 修改：`User/usart/bsp_usart.h/.c`
- 修改：`Project/RVMDK（uv5）/Fire_F103C8.uvprojx`

- [ ] **步骤 1：定义测距滤波接口**

```c
typedef struct {
    uint16_t distance_mm;
    uint8_t valid;
} RangeSample;

void RangeFilter_Init(void);
RangeSample RangeFilter_Update(uint16_t raw_mm, uint8_t raw_valid);
```

对 `0`、超量程和明显跳变样本标记无效；有效样本使用整数一阶滤波，避免在 F103 上引入不必要的浮点运算。

- [ ] **步骤 2：定义跟随状态接口**

```c
void FollowControl_Init(void);
void FollowControl_Reset(void);
void FollowControl_SetEnabled(uint8_t enabled);
int16_t FollowControl_Update(uint16_t distance_mm,
                             uint8_t distance_valid,
                             uint32_t dt_ms);
```

目标距离固定为 `250 mm`，输出目标 RPM 限制为 `0..CONTROL_FOLLOW_MAX_RPM`。`FollowControl_SetEnabled(0)` 是默认状态；跟随未启用、距离无效或距离小于等于 `150 mm` 时输出 0。该模块只产生建议值，不调用任何电机函数。

- [ ] **步骤 3：定义 UART 发送接口**

```c
void RangeUart_Init(void);
void RangeUart_Send(const RangeStatusFrame *frame);
```

`RangeUart_Send()` 使用 USART1 发送协议固定 9 字节帧；`printf` 不得用于控制数据。发送周期为 40 ms。

- [ ] **步骤 4：接入测距 ECU 主循环**

测距 ECU 分支初始化 `VL53L0X` 所需 SysTick 和 USART1，每 40 ms 调用 `Distance_Update(1)`，经滤波和跟随 PID 后发送距离、建议目标 RPM、状态位。主循环不得等待主 ECU 应答。

- [ ] **步骤 5：测试测距算法**

添加距离在 250 mm 两侧的 PID 方向、150 mm 停车、无效距离停车和滤波跳变抑制测试。主机测试期望输出 `range/follow tests passed`。

- [ ] **步骤 6：提交测距 ECU**

```powershell
git add User/ranging User/main.c User/usart Project/RVMDK（uv5）/Fire_F103C8.uvprojx tests/control/test_control.c
git commit -m "feat: add ranging ecu follow control"
```

## 任务 5：主 ECU 非阻塞链路与安全仲裁

**文件：**

- 修改：`User/si24r1/drv_periph/inc/drv_RF24L01.h`
- 修改：`User/si24r1/drv_periph/src/drv_RF24L01.c`
- 修改：`User/usart/bsp_usart.h/.c`
- 修改：`User/stm32f10x_it.c`
- 创建：`User/control/vehicle_control.h/.c`
- 创建：`User/control/speed_feedback.h/.c`
- 修改：`User/main.c`
- 修改：`Project/RVMDK（uv5）/Fire_F103C8.uvprojx`

- [ ] **步骤 1：添加 NRF24L01 非阻塞接收**

新增 `uint8_t NRF24L01_RxPacket_NonBlocking(uint8_t *rxbuf)`：只检查 STATUS 的 `RX_OK` 位，有数据就读出并返回长度，没有数据立即返回 0；不得复用现有会等待 3 秒的阻塞 `NRF24L01_RxPacket()`。保留原函数供兼容调试。

- [ ] **步骤 2：添加 USART1 环形缓冲区**

在 `bsp_usart.c` 中定义 128 字节接收环形缓冲区，提供：

```c
void USART_RxBuffer_Init(void);
uint8_t USART_RxBuffer_Pop(uint8_t *data);
```

在 `USART1_IRQHandler()` 中读取 `USART_ReceiveData(USART1)` 并写入缓冲区；溢出时丢弃最旧字节并设置溢出标志。主 ECU 通过 `VehicleProtocol_DecodeRange()` 逐字节重组固定帧。

- [ ] **步骤 3：定义主 ECU 控制数据结构**

`vehicle_control.h` 使用：

```c
typedef enum {
    THROTTLE_DIRECT_PWM = 1,
    THROTTLE_TARGET_SPEED_PID = 2
} ThrottleMode;

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
    int16_t follow_target_rpm;
    uint8_t follow_target_valid;
} VehicleFeedback;

typedef struct {
    uint8_t direction;
    uint8_t left_duty;
    uint8_t right_duty;
    uint8_t emergency_stop;
} DriveCommand;
```

- [ ] **步骤 4：实现两种油门和主 ECU PID**

模式 1 用 `pedal * 100 / 255` 生成 PWM；模式 2 用踏板映射目标 RPM，只有 `speed_valid` 为真时调用左右速度 PID，否则停车。左右轮先使用同一目标和同一 PWM，差速转向留到后续任务。

- [ ] **步骤 5：实现主 ECU AEB 和跟随仲裁**

控制优先级必须严格为：无线超时停车；前进且距离 `<=150 mm` 停车；跟随模式距离无效停车；N 档或零踏板停车；跟随有效时使用 `follow_target_rpm`；否则使用手动踏板目标。跟随启用状态初值为 0，当前没有任何无线字段、按键或开关调用启用函数。

- [ ] **步骤 6：实现速度反馈占位接口**

```c
void SpeedFeedback_Init(void);
void SpeedFeedback_Get(int16_t *left_rpm,
                       int16_t *right_rpm,
                       uint8_t *valid);
```

FG 引脚未确认前返回 `valid=0` 和 RPM 0，使模式 2 安全停车。不得伪造有效转速。

- [ ] **步骤 7：接入主 ECU 10 ms 控制循环**

主 ECU 初始化控制模块、UART 接收和 1 ms 时间基准；每 10 ms 执行非阻塞无线轮询、测距帧解析、超时检查、`VehicleControl_Update()` 和电机输出。OLED、灯光、舵机刷新不能使用长时间阻塞延时。

- [ ] **步骤 8：运行主 ECU 纯逻辑测试并提交**

测试覆盖模式 1、模式 2 无效反馈停车、D/R/N、AEB 临界值、倒车逃离、测距帧超时和跟随默认关闭。通过后提交：

```powershell
git add User/control/vehicle_control.* User/control/speed_feedback.* User/main.c User/usart User/stm32f10x_it.c User/si24r1/drv_periph Project/RVMDK（uv5）/Fire_F103C8.uvprojx tests/control/test_control.c
git commit -m "feat: add main ecu safety arbitration"
```

## 任务 6：主 ECU 基础执行器、显示与灯光

**文件：**

- 修改：`User/main.c`
- 修改：`User/ws2812b/ws2812b.c`
- 修改：`User/oled/oled.c`（只在需要新增显示页面时）
- 修改：`User/turn/turn.c`（只在需要修正映射时）

- [ ] **步骤 1：接入方向盘到舵机映射**

对合法无线帧调用 `map_wheel(rx_buffer[1] * 256 + rx_buffer[2])` 和 `Turn_SetDuty()`；无效帧保持上一次合法转角，启动时保持 50% 归中。

- [ ] **步骤 2：接入灯光逻辑**

左右转向使用 `WS2812_Blink(left, right, r, g, b, delay)`；R 档点亮倒挡灯。灯光调用不能改变电机方向或 PWM。

- [ ] **步骤 3：接入 OLED 页面**

每 100 ms 显示组号、D/R/N、左右转向、当前距离、AEB 状态和跟随状态。上电后允许等待主 ECU Reset 完成初始化，但不得用 OLED 刷新阻塞 10 ms 控制任务。

- [ ] **步骤 4：架空后轮完成基础硬件验证**

按 N 档、前进、后退、舵机归中、左右灯、150 mm 内 AEB、手动倒车顺序验证。主 ECU 下载时断开 12 V 主开关，不进行热插拔。

- [ ] **步骤 5：提交基础执行器**

```powershell
git add User/main.c User/ws2812b User/oled User/turn
git commit -m "feat: connect main ecu actuators"
```

## 任务 7：补充文档和最终验证

**文件：**

- 修改：`README.md`
- 修改：`docs/superpowers/specs/2026-09-08-throttle-follow-control-design.md`（只记录已实现接口和默认配置）
- 测试：`tests/control/test_control.c`

- [ ] **步骤 1：更新 README 使用说明**

写清三种宏配置：方向盘端定义 `__RF24L01_TX_TEST__`；主 ECU 定义 `_MAIN_ECU_` 且不定义前者；测距 ECU 两者都不定义。写清方向盘 ECU 与主 ECU 必须使用同一组 NRF 地址，组间地址不得重复；测距 ECU 不参与 NRF24L01 通信。

- [ ] **步骤 2：执行主机测试**

```powershell
cc -std=c89 -Wall -Wextra -IUser\protocol -IUser\control -IUser\ranging tests\control\test_control.c User\protocol\vehicle_protocol.c User\control\pid.c User\control\vehicle_control.c User\ranging\range_filter.c User\ranging\follow_control.c -o tests\control\test_control.exe
tests\control\test_control.exe
```

期望所有测试通过，覆盖协议、PID、两种油门、AEB、跟随无效数据和状态复位。

- [ ] **步骤 3：执行 Keil 三角色编译**

分别设置三种宏组合并执行 Rebuild：方向盘 ECU、主 ECU、测距 ECU。每种角色都必须无编译和链接错误；确认 `.hex` 输出更新时间与本次源码一致。

- [ ] **步骤 4：检查工程差异**

```powershell
git diff --check
git status --short
```

工作区只允许保留预期源码、文档和测试，不提交 `Project/Output` 下的构建产物。

- [ ] **步骤 5：提交文档与验证记录**

```powershell
git add README.md docs/superpowers/specs/2026-09-08-throttle-follow-control-design.md tests/control/test_control.c
git commit -m "docs: document three-ecu build and verification"
```

## 计划自检

- 课件基础功能：无线显示、前进后退转向、灯光、测距显示、AEB 和组号显示分别由任务 2、任务 5、任务 6 覆盖。
- 课件进阶功能：定距跟随由任务 4 和任务 5 预留并实现，默认关闭；轮速采样和差速转向暂不伪造硬件参数，保留任务 5 的反馈接口，待 FG 引脚、减速比和轮径确认后单独扩展。
- 用户指定参数：AEB 150 mm、跟随 250 mm、FG 18 脉冲/转、油门模式 1/2 均已进入协议或控制配置。
- 安全边界：测距 ECU 不直接控制电机，主 ECU 统一执行 AEB、超时和最终 PWM 限幅；倒车逃离仅在手动模式允许。
- 默认行为：跟随不可由当前无线帧触发，默认 `follow_enabled=0`，默认油门模式为 1。
