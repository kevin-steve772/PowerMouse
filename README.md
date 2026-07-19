# PowerMouse

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

一个轻量的鼠标连点器，专为 Minecraft 生电（生存/技术向）挂机设计。

**不是 PVP 辅助，没有反反作弊，只是一个简单的自动化工具。**

**请勿将其用于 PVP ，如果被服务器封号，我们概不负责。**

## 特性

- **后台运行** — 启动后藏在系统托盘，不占屏幕空间
- **全局热键** — 按 `F6` 启动/停止连点，无需切出游戏
- **左右键切换** — 支持左键、右键、左右同时点击
- **CPS 可调** — 1–100 CPS，通过命令行或图形界面设置
- **托盘菜单** — 右键图标可操作启动/停止、选项、退出
- **选项界面** — 可在运行时修改设置，即时生效
- **Toast 通知** — 状态变化时有原生气泡提示
- **跨平台** — Windows 原生 / Linux X11

## 用法

```
PowerMouse [选项]

选项:
  --cps N      每秒点击次数 (1-100, 默认 8)
  --left       仅左键点击 (默认)
  --right      仅右键点击
  --both       左右键同时点击
  --help       显示帮助
```

**热键**: `F6` — 启动/停止

**托盘右键菜单**: 启动/停止 · 选项 · 退出

### 示例

```bash
PowerMouse                  # 默认左键 8 CPS
PowerMouse --both           # 左右键同时点
PowerMouse --right --cps 12 # 右键 12 CPS
```

## 从源码编译

### Windows (LLVM-MinGW)

```bash
clang++ -std=c++17 -mwindows -D_WIN32_WINNT=0x0600 -O2 main.cpp \
  -o PowerMouse.exe -luser32 -lshell32 -lgdi32 -lcomctl32
```

### Linux (X11)

```bash
# 依赖: libxtst-dev, notify-send (通常已预装)
cmake -B build && cmake --build build
```

## Minecraft 生电场景

| 场景 | 推荐设置 |
|------|----------|
| 自动钓鱼 | `--right --cps 1` |
| 刷怪塔挂机 | `--left --cps 8` |
| 作物收割 (右键) | `--right --cps 6` |
| 盾牌+剑切换 | `--both --cps 10` |

## 许可

[MIT](LICENSE)
