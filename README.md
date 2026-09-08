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
