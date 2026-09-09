# 410car

STM32F103C8 小车控制工程，使用 Keil MDK 5（RVMDK/UV5）和 STM32F10x 标准外设库。

## 快速开始

1. 克隆仓库后，用 Keil 打开 `Project/RVMDK（uv5）/Fire_F103C8.uvprojx`。
2. 编译产物会生成在 `Project/Output`；该目录不会提交到 Git。
3. 个人 Keil 配置（如 `.uvoptx`、`.uvguix.*`）同样不会提交，避免团队成员互相覆盖设置。

## 协作约定

- `main` 保持可编译；每项工作在 `feature/<姓名>-<主题>` 分支完成。
- 提交信息写清改动，例如：`feat: 添加超声波避障`、`fix: 修正 CAN 初始化`。
- 推送分支后发起合并请求（Pull Request），至少由一名队友检查后合并。
- 合并前先同步 `main`，并确认工程能够正常编译。

## 主 ECU 距离显示

在 `User/main.h` 中保持 `_MAIN_ECU_` 已定义、`__RF24L01_TX_TEST__` 未定义，
使用 Keil 打开 `Project/RVMDK（uv5）/Fire_F103C8.uvprojx` 并执行 Rebuild。
将生成的 `Project/Output/can.hex` 烧录到车载主 ECU 即可；测距 ECU 和方向盘 ECU
继续使用原固件。本次构建另存的 `Project/Output/main_ecu_distance.hex` 是主 ECU 固件副本，
后续修改源码重新编译时，以重新生成的 `can.hex` 为准。
当前正常接收版本为 D3，右上角显示 `D3`；本次构建也保存为
`Project/Output/main_ecu_distance_v3.hex`。使用原主 ECU 工程和 `can` 目标下载即可。

- 测距 ECU 的 USART1 TX（PA9）连接主 ECU USART1 RX（PA10），两板共地。
- 串口配置：115200 波特率，8 数据位、无校验、1 停止位。
- 兼容原测距程序的纯数字毫米行，例如 `356\n` 或 `356\r\n`，不需要主 ECU 发起请求。
- OLED 第二行第 6 列显示 `D:  356mm`；保留组号、日期、挡位、转向状态和轮速显示。
- 每 200 ms 检查并仅重画变化的距离字段，数据有效期为 500 ms。D3 用下表状态替代笼统的横线。
  屏幕可见变化还需等待最多一个 200 ms 刷新周期。
- 初始化调试文字、超过五位的数字及超出 65535 的数值会被丢弃，不会刷新有效距离的时间戳。
  上电、接收溢出或串口错误后，先等待行结束再接受下一条完整数据。
- 每轮最多解析 32 个串口字节，距离/轮速显示在控制输出之后执行；普通字符串按上下两页
  批量写 OLED，9 字符距离字段为 8 次 I²C 传输，避免逐字节起停总线拖慢无线轮询。
- 接收不依赖方向盘无线数据；现有启动过程仍要求车端 NRF24L01 模块连接正常。
- D3 修复了慢速帧的解析：上一行完整结束后，即使间隔达到 500 ms，也接受下一条完整新帧；
  只有尚未结束的残缺行在长间隔后被丢弃。低于 2 Hz 的数据会在新值与超时提示之间交替，
  不再因为这个间隔被永久拒收。

| 距离区显示 | 含义 |
|---|---|
| `D:  356mm` | 收到有效的毫米值 |
| `D:NO RX` | 主 ECU 尚未获得任何接收字节/错误事件 |
| `D:WAIT` | 等待行结束或重新同步后的完整数据 |
| `D:ZERO` | 测距板发送数值 0，原驱动用它表示测量失败 |
| `D:FORMAT` | 收到的内容不符合完整纯数字行，或数字超范围 |
| `D:RX ERR` | UART 接收错误、丢字节或缓冲溢出后等待恢复 |
| `D:STALE` | 数据超时，空行也不会续期旧读数 |

参数位于 `User/distance_display/distance_display.h`。
现有测距协议没有校验和或 VL53L0X `RangeStatus`，因此屏幕读数不能保证排除所有物理测量异常。

主机测试命令（需要 MSVC Build Tools，可用 `-VcVars` 指定环境脚本路径）：

```powershell
.\tests\distance_display\run_tests.ps1
```

实车验收：车辆静止时移动挡板，确认毫米数随距离变化；断开测距串口后等待约
0.5～0.7 秒确认 `D:STALE`，重新连接确认数字恢复；关闭方向盘端，确认距离仍更新。
主机测试及编译无法替代这些硬件验证。

### 历史定位固件（当前先处理 D3 测距显示）

实车报告第一版距离始终显示横线，并且遥控失灵。软件测试确认第一版存在显示总线开销大、
串口处理没有单轮工作上限两项问题，已修正；但距离始终为横线时不会重复重画该字段，
所以这些修正尚不能单独证明实车失灵的根因。

- `Project/Output/main_ecu_distance_v2.hex`：历史 V2 接收版本，已修正调度与刷屏开销。
- `Project/Output/main_ecu_uart_off.hex`：与 V2 同源的主 ECU 定位版本，仅不调用测距接收中断
  启用函数，初始距离字段显示 `UART OFF`。它不显示距离，只用于比较遥控能否正常工作。
- 对照版独立 Keil 工程位于
  `tests/distance_display/build/uart-off-main/Project/RVMDK（uv5）/MAIN_ECU_UART_OFF.uvprojx`，
  目标名称为 `MAIN_ECU_UART_OFF`，产物为 `main_ecu_uart_off.axf/.hex`。
  此本地诊断工程沿用原工程的 fireDAP 下载器配置。下载日志应指向 `main_ecu_uart_off.axf`；
  `Verify OK` 只确认写入校验，下载后还应复位主 ECU，确认第二行出现 `UART OFF`。
- 两版均已通过主 ECU Keil 全量编译（0 错误、0 警告），尚需实车比较。两版之间切换时保持
  同一套接线，只烧录主 ECU；只有比较 V2 与 UART OFF，才能单独检验接收中断的影响。

回归测试还检查了真实 OLED 软件 I²C 输出的事务数量、字模、右边界裁剪与相邻字段保持，
以及主 ECU 距离任务面对持续输入时是否按预算退出。测试通过不代表已经定位或解决实车失灵。
