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

- 测距 ECU 的 USART1 TX（PA9）连接主 ECU USART1 RX（PA10），两板共地。
- 串口配置：115200 波特率，8 数据位、无校验、1 停止位。
- 兼容原测距程序的纯数字毫米行，例如 `356\n` 或 `356\r\n`，不需要主 ECU 发起请求。
- OLED 第二行第 6 列显示 `D:  356mm`；保留组号、日期、挡位、转向状态和轮速显示。
- 每 200 ms 检查并仅重画变化的距离字段；数据满 500 ms 未更新、传感器返回 0
  或 UART 检测到丢字节时显示 `D:-----mm`。屏幕可见变化还需等待最多一个 200 ms 刷新周期。
- 初始化调试文字、超过五位的数字及超出 65535 的数值会被丢弃，不会刷新有效距离的时间戳。
  上电、接收溢出或串口错误后，先等待行结束再接受下一条完整数据。
- 接收不依赖方向盘无线数据；现有启动过程仍要求车端 NRF24L01 模块连接正常。

参数位于 `User/distance_display/distance_display.h`。
现有测距协议没有校验和或 VL53L0X `RangeStatus`，因此屏幕读数不能保证排除所有物理测量异常。

主机测试命令（需要 MSVC Build Tools，可用 `-VcVars` 指定环境脚本路径）：

```powershell
.\tests\distance_display\run_tests.ps1
```

实车验收：车辆静止时移动挡板，确认毫米数随距离变化；断开测距串口后等待约
0.5～0.7 秒确认横线，重新连接确认数字恢复；关闭方向盘端，确认距离仍更新。
主机测试及编译无法替代这些硬件验证。
