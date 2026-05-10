# HomeDevice 项目超详细新手 README（从 0 到跑通）

> 适合对象：第一次接触嵌入式、后端、APP 联调的同学。
> 
> 目标：照着本文一步一步做，完成“传感器采集 -> ESP32 上传 -> 服务器存储/下发命令 -> 手机查看与控制 -> 短信告警”的完整闭环。

---

## 1. 这个项目到底是什么

`HomeDevice` 是一个室内安防/环境监测系统，包含 4 个部分：

1. `stm32`：负责读取传感器、控制蜂鸣器和继电器、驱动 OLED、触发短信。  
2. `ESP32`：负责联网，把 STM32 数据发到服务器，并把服务器命令转发回 STM32。  
3. `Server`：FastAPI 后端，保存状态/告警，提供 APP 接口，维护 WebSocket 推送。  
4. `HbuilderX`：uni-app 手机端，用于实时查看状态、历史告警、下发控制命令。

你可以把它理解成：

- STM32 = 现场控制器
- ESP32 = 网关
- Server = 云端大脑
- 手机端 = 操作面板

---

## 2. 总体架构和数据流

## 2.1 硬件/软件架构图

```text
[温湿度/烟雾/火焰/人体传感器]
              |
              v
      [STM32F103C8T6]
       |   |        \
       |   |         \__ [Air780E 短信模块]
       |   |             (USART2, AT)
       |   |
       |   +--> [继电器/蜂鸣器/OLED/按键]
       |
       | USART1 (串口协议 AA 55 ...)
       v
       [ESP32-C3]
           |
           | Wi-Fi + HTTP/WS
           v
      [FastAPI Server + SQLite]
           |
           | HTTP + WebSocket
           v
      [uni-app 手机端]
```

## 2.2 上行数据流（设备 -> 云）

1. STM32 周期采样传感器。  
2. STM32 组帧 `AA 55 TYPE LEN DATA CHECK` 发给 ESP32。  
3. ESP32 解析帧并缓存状态。  
4. ESP32 `POST /api/device/upload` 上传到服务器。  
5. 服务器更新 `devices` 表；若含告警则写入 `alarms` 并通过 WebSocket 推送给 APP。  
6. APP 轮询 + WebSocket 获取最新状态。

## 2.3 下行控制流（手机 -> 设备）

1. APP 调用 `POST /api/app/control` 下发命令。  
2. 服务器写入 `commands` 表，状态 `PENDING`。  
3. ESP32 轮询 `GET /api/device/cmd` 取到命令（服务器改为 `SENT`）。  
4. ESP32 把命令转成 UART 帧发给 STM32。  
5. STM32 执行动作并回 `ACK` 帧。  
6. ESP32 下次上传时带 `ack_command_id`，服务器把命令改成 `DONE`。

---

## 3. 通信方式总表

| 链路 | 协议 | 方向 | 作用 |
|---|---|---|---|
| STM32 <-> ESP32 | UART 串口帧 | 双向 | 传状态、收控制、回 ACK |
| STM32 <-> Air780E | UART AT 指令 | STM32->Air780E | 初始化与发短信 |
| ESP32 -> Server | HTTP POST | 上行 | 上传状态/告警/ACK |
| ESP32 <- Server | HTTP GET | 下行 | 轮询拉取待执行命令 |
| Server -> 手机端 | WebSocket | 推送 | 实时状态与告警 |
| 手机端 -> Server | HTTP | 请求 | 登录、查状态、查历史、下控制 |

---

## 4. 仓库目录全解（含子目录）

> 说明：本项目有“业务代码目录”和“编译产物目录”。初学者先看业务代码。

## 4.1 顶层目录

```text
HomeDevice/
├─ docs/              # 文档目录（本 README 在这里）
├─ ESP32/             # ESP32-C3 网关工程（主用 ESP-IDF）
├─ HbuilderX/         # uni-app 手机端工程
├─ Server/            # FastAPI 后端工程
└─ stm32/             # STM32F103 固件工程（Keil MDK）
```

## 4.2 `ESP32/` 目录

### 业务关键文件

- `ESP32/CMakeLists.txt`  
  ESP-IDF 工程入口，项目名 `home_device_gateway`。  
- `ESP32/main/CMakeLists.txt`  
  注册 `main.c` 并声明依赖组件（Wi-Fi、HTTP、JSON、UART 等）。  
- `ESP32/main/main.c`  
  主业务代码：Wi-Fi、HTTP 上传、命令轮询、UART 解析与转发。  
- `ESP32/README.md`  
  ESP32 子工程说明。  
- `ESP32/sdconfig` 与 `sdkconfig.defaults`  
  ESP-IDF 配置（芯片目标、Flash、串口波特率等）。

### 历史/兼容文件

- `ESP32/platformio.ini`
- `ESP32/src/main.cpp`

这两者是早期 Arduino 路线。当前主线是 **ESP-IDF + `main/main.c`**。

### 编译产物目录

- `ESP32/build/`：CMake/Ninja/二进制产物，不建议手动改。

## 4.3 `stm32/` 目录

### 业务关键子目录

- `stm32/User/`
  - `main.c`：入口，仅调用 `HomeDevice_AppInit/AppLoop`。
  - `stm32f10x_it.c`：中断处理，`SysTick_Handler` 每 1ms 调 `HomeDevice_Tick1ms()`。
- `stm32/BSP/`（**核心业务代码**）
  - `home_device_app.c/.h`：主流程调度、告警逻辑、命令执行、状态上传帧。
  - `drivers_port.c`：硬件引脚与外设初始化（GPIO/UART/ADC/SysTick 等）。
  - `protocol.c/.h`：串口帧协议实现。
  - `comm_uart.c/.h`：UART1(ESP32) + UART2(Air780E) 通信封装。
  - `alarm_logic.c/.h`：烟雾/火焰/入侵告警判定。
  - `sensor.c/.h`：传感器读取封装。
  - `scheduler.c/.h`：任务周期调度器。
  - `key_input.c/.h`：按键去抖与事件。
  - `oled_ui.c/.h`：OLED 页面显示逻辑。
  - `air780e_sms.c/.h`：短信模块 AT 命令发送。
  - `app_config.h`：阈值、任务周期、手机号等配置。
  - `app_types.h`：系统数据结构定义。
- `stm32/Start/`：启动文件、系统时钟文件、CMSIS 核心文件。
- `stm32/Library/`：STM32 SPL 标准外设库。

### 工程文件

- `stm32/Project.uvprojx`：Keil 工程文件（真实编译清单在这里）。
- `stm32/Project.uvoptx`：Keil 调试/选项文件。
- `stm32/keilkill.bat`：清理脚本。

### 说明与历史目录

- `stm32/Inc/` 与 `stm32/Src/`：保留的历史/模板结构（与 BSP 有重复），目前主线构建用 `BSP + User`。  
- `stm32/Objects/`、`Listings/`、`DebugConfig/`、`build.log`：构建/调试产物与日志。

## 4.4 `Server/` 目录

```text
Server/
├─ requirements.txt
├─ README.md
└─ app/
   ├─ main.py
   ├─ core/
   │  ├─ config.py
   │  ├─ db.py
   │  ├─ security.py
   │  └─ ws_manager.py
   ├─ models/
   │  └─ schemas.py
   └─ routers/
      ├─ app_api.py
      ├─ device_api.py
      └─ ws_api.py
```

### 文件职责

- `app/main.py`：FastAPI 入口，挂路由、启动时建库。
- `core/config.py`：默认配置（默认账号、默认设备 ID、DB 文件名）。
- `core/db.py`：SQLite 表结构与增删改查。
- `core/security.py`：SHA256 密码哈希。
- `core/ws_manager.py`：WebSocket 客户端管理和广播。
- `models/schemas.py`：Pydantic 请求模型。
- `routers/device_api.py`：设备上传与命令下发给设备。
- `routers/app_api.py`：APP 登录/状态/控制/历史。
- `routers/ws_api.py`：`/ws/alarm` WebSocket。

## 4.5 `HbuilderX/` 目录

```text
HbuilderX/
├─ App.vue
├─ main.js
├─ manifest.json
├─ pages.json
├─ common/
│  ├─ config.js
│  └─ api.js
├─ pages/
│  ├─ index/index.vue
│  ├─ history/history.vue
│  ├─ control/control.vue
│  └─ settings/settings.vue
└─ .hbuilderx/launch.json
```

### 文件职责

- `common/config.js`：保存后端地址和设备 ID（本地存储）。
- `common/api.js`：封装 HTTP 与 WebSocket 调用。
- `pages/index/index.vue`：实时总览（状态 + 连接状态 + 最新告警）。
- `pages/history/history.vue`：历史告警列表。
- `pages/control/control.vue`：模式/继电器/静音/阈值下发。
- `pages/settings/settings.vue`：服务器地址和设备 ID 设置。

## 4.6 `docs/` 目录

- 当前文档目录。建议把后续接线图、流程图、排障手册都放这里。

---

## 5. 关键模块与功能说明（按系统分层）

## 5.1 STM32 侧

### `home_device_app`

作用：整个设备端“业务大脑”。

主要功能：

1. 初始化所有模块。  
2. 周期调度：按键、采样、显示、上传帧。  
3. 告警判定并触发：
   - 向 ESP32 发告警帧。
   - 向 Air780E 发短信。  
4. 处理 ESP32 下发命令，并回 ACK。

### `scheduler`

按周期触发任务位：

- `TASK_KEY`：20ms
- `TASK_SENSOR`：200ms
- `TASK_OLED`：500ms
- `TASK_UPLOAD`：1000ms

### `alarm_logic`

- 烟雾：超过阈值并连续达到计数才触发（抗抖动）
- 火焰：火焰输入为真即触发
- 入侵：仅在布防模式下触发

### `comm_uart + protocol`

实现固定帧协议：

- 头：`0xAA 0x55`
- 类型：1 字节
- 长度：1 字节
- 数据：`LEN` 字节
- 校验：`(type + len + data求和) & 0xFF`

### `drivers_port`

封装硬件细节（GPIO/UART/ADC/按键/OLED），业务层不直接碰寄存器。

### `air780e_sms`

- 初始化发送 AT 探测与文本模式配置。
- 告警时发送短信内容并发 `Ctrl+Z(0x1A)` 结束。

## 5.2 ESP32 侧

### 主循环职责

1. 处理 UART 字节流并解析帧。  
2. 每秒上传一次设备状态到服务器。  
3. 每秒轮询一次服务器命令。  
4. 命令转串口帧发给 STM32。  
5. 打印诊断日志（wifi/valid/upload_fail/cmd 等）。

### 为什么会出现 `valid=0`

`valid` 只有在收到 STM32 的环境帧后才会置真。没收到首帧前，即使 Wi-Fi 已连通，也不会进行有效上传。

## 5.3 Server 侧

### 数据库表

- `users`：登录用户
- `devices`：设备最新状态
- `alarms`：告警历史
- `commands`：待执行/已发送/已完成命令

### API 模块

- `device_api`：设备上传、设备拉命令
- `app_api`：APP 登录、查状态、下命令、查历史
- `ws_api`：实时推送

## 5.4 APP 侧

- `index`：展示实时状态（轮询 + WebSocket）
- `history`：查看告警历史
- `control`：下发控制命令
- `settings`：改服务器地址与设备 ID

## 5.5 STM32 模块逐个解释（更细）

| 模块名 | 主要文件 | 它负责什么 | 它和谁通信 |
|---|---|---|---|
| 应用主控 | `BSP/home_device_app.c` | 组织全部业务流程；执行调度任务；处理命令；触发告警 | 调 `drivers`、`scheduler`、`sensor`、`alarm_logic`、`comm_uart`、`air780e_sms` |
| 调度器 | `BSP/scheduler.c` | 按毫秒计时产生任务位（按键/采样/OLED/上传） | 供 `home_device_app` 查询 |
| 传感器层 | `BSP/sensor.c` | 快速采样（烟雾/火焰/入侵）和慢速采样（温湿度） | 调 `drivers_port` 的硬件读函数 |
| 告警逻辑 | `BSP/alarm_logic.c` | 按阈值和模式生成烟雾/火焰/入侵告警位，并给出上升沿 | 输入传感器 + 设备状态，输出 `AlarmResult` |
| 按键模块 | `BSP/key_input.c` | 4 键去抖，输出 `KEY_EVT_K1..K4` 事件位 | 给 `home_device_app` |
| OLED UI | `BSP/oled_ui.c` | 生成 3 个页面文案，只刷新变化行（减少闪烁） | 调 `Drivers_OledPrint` |
| 串口协议 | `BSP/protocol.c` | 帧组包/解包和校验，协议头 `AA 55` | 被 `comm_uart` 和 `home_device_app` 调用 |
| 串口通信 | `BSP/comm_uart.c` | UART1 与 ESP32 收发帧；UART2 与 Air780E 发 AT 文本/原始字节 | 连 ESP32、Air780E |
| 短信模块 | `BSP/air780e_sms.c` | 发 AT 初始化、发短信正文、发 `0x1A` 结束 | 经 UART2 连 Air780E |
| 硬件驱动 | `BSP/drivers_port.c` | GPIO/UART/ADC/SysTick/OLED 等底层初始化与读写 | 直连 MCU 外设 |

### 5.5.1 `home_device_app` 主流程（代码级）

1. `HomeDevice_AppInit()`：初始化驱动、状态、传感器、按键、OLED、串口、短信模块、调度器。  
2. `HomeDevice_AppLoop()`：每次循环先 `CommUart_Poll()`，再按任务位执行：  
   - `TASK_KEY`：处理 K1~K4  
   - `TASK_SENSOR`：采样 + 告警评估 + 执行输出  
   - `TASK_OLED`：刷新 OLED  
   - `TASK_UPLOAD`：发状态帧给 ESP32  
3. 收到 ESP32 命令帧后：
   - 控制类（`PROTO_TYPE_CTRL`）执行模式/继电器/静音
   - 参数类（`PROTO_TYPE_PARAM`）设置烟雾阈值
   - 处理完都回 ACK 帧给 ESP32

### 5.5.2 告警与输出行为

1. 烟雾：`smoke >= threshold` 连续达到计数后触发。  
2. 火焰：有火焰输入即触发。  
3. 入侵：只有在 `MODE_ARM` 下才触发。  
4. 输出策略：
   - 烟雾或火焰告警会强制开继电器（喷淋）
   - 任意告警会让蜂鸣器响
   - 若用户按静音，蜂鸣器可被静音

## 5.6 ESP32 模块逐个解释（`main/main.c`）

| 功能块 | 主要函数 | 作用 |
|---|---|---|
| Wi-Fi 事件处理 | `wifi_event_handler` | 连接/断开重连/拿 IP 状态维护 |
| UART 初始化 | `uart_init` | 配置 UART1 引脚与参数 |
| 串口解析 | `process_uart` + `handle_frame` | 收 STM32 帧，更新状态与统计计数 |
| 上传接口 | `post_upload` | 组 JSON 上报 `/api/device/upload` |
| 命令轮询 | `poll_command` | 拉 `/api/device/cmd` 并转发给 STM32 |
| 命令编码 | `send_command_to_stm32` | 把字符串命令转成控制/参数帧 |
| HTTP 统一请求 | `http_request` | GET/POST 请求封装 |
| 主循环 | `app_main` while(1) | 周期执行：串口处理 + 上传 + 命令轮询 + 诊断日志 |

## 5.7 Server 模块逐个解释

| 模块 | 文件 | 功能 |
|---|---|---|
| 应用入口 | `app/main.py` | 创建 FastAPI，挂载路由，启动时初始化数据库 |
| 配置 | `core/config.py` | 默认数据库路径、默认设备 ID、默认账号密码 |
| 数据层 | `core/db.py` | 建表、用户校验、设备状态更新、告警记录、命令队列状态流转 |
| 安全 | `core/security.py` | SHA256 密码哈希 |
| WebSocket 管理 | `core/ws_manager.py` | 维护连接列表并广播告警/状态 |
| 设备 API | `routers/device_api.py` | 设备上传、设备拉命令 |
| APP API | `routers/app_api.py` | 登录、查状态、下命令、查历史 |
| WS API | `routers/ws_api.py` | `/ws/alarm` 长连接接口 |

## 5.8 APP 模块逐个解释

| 页面/模块 | 文件 | 功能 |
|---|---|---|
| 全局入口 | `App.vue` / `main.js` | uni-app 应用启动 |
| API 封装 | `common/api.js` | HTTP 请求 + WebSocket 连接封装 |
| 本地配置 | `common/config.js` | 保存 `baseUrl` 和 `deviceId` |
| 总览页 | `pages/index/index.vue` | 状态展示、连接状态、最新告警 |
| 历史页 | `pages/history/history.vue` | 显示告警历史列表 |
| 控制页 | `pages/control/control.vue` | 下发布防/撤防/继电器/静音/阈值 |
| 设置页 | `pages/settings/settings.vue` | 修改后端地址、设备 ID、测试连接 |

## 5.9 OLED 每页字母和按键解释（按实际代码）

> OLED 是 4 行 x 16 列字符屏。页面来自 `BSP/oled_ui.c`，按键行为来自 `BSP/home_device_app.c`。

### 5.9.1 Page 0：实时总览（`P0`）

```text
行1: P0 M:<mode> A:<Y/N>
行2: T:<temp> H:<hum> S:<smoke>
行3: F:<flame> I:<intrusion> R:<relay> B:<buzzer>
行4: K1Pg K2Md
```

字母含义：

- `P0`：第 0 页  
- `M`：模式（`DISARM/ARM/AUTO`）  
- `A`：是否有告警（`Y` 有，`N` 无）  
- `T`：温度  
- `H`：湿度  
- `S`：烟雾值  
- `F`：火焰状态  
- `I`：入侵状态  
- `R`：继电器状态  
- `B`：蜂鸣器状态  
- `K1Pg`：按 K1 换页  
- `K2Md`：按 K2 切换模式

### 5.9.2 Page 1：烟雾阈值页（`P1`）

```text
行1: P1 THRESHOLD
行2: TH:<threshold> NOW:<smoke_now>
行3: K3:+10 K4:-10
行4: K1Pg K2Md
```

字母含义：

- `TH`：当前烟雾告警阈值  
- `NOW`：当前烟雾值  
- `K3:+10`：K3 每次阈值 +10（上限 4095）  
- `K4:-10`：K4 每次阈值 -10（下限 50）

### 5.9.3 Page 2：控制页（`P2`）

```text
行1: P2 CONTROL
行2: M:<mode> R:<relay> B:<buzzer>
行3: K3:RelayOn
行4: K4:Silence K1
```

字母含义：

- `CONTROL`：控制页  
- `K3:RelayOn`：按 K3 手动打开继电器  
- `K4:Silence`：按 K4 静音（并在无告警时释放手动继电器）

### 5.9.4 四个按键全局行为总结

1. **K1**：在 `P0 -> P1 -> P2 -> P0` 循环切页。  
2. **K2**：模式循环切换 `DISARM -> ARM -> AUTO -> DISARM`。  
3. **K3**：  
   - 在 `P1`：烟雾阈值 +10  
   - 在 `P0/P2`：手动开继电器  
4. **K4**：  
   - 在 `P1`：烟雾阈值 -10  
   - 在 `P0/P2`：静音；若当前无告警，还会释放手动继电器

### 5.9.5 模式解释（`M`）

1. `DISARM`：撤防，入侵不报警。  
2. `ARM`：布防，入侵会报警。  
3. `AUTO`：自动模式，代码中由 `Drivers_IsNightTime()` 决定白天/夜晚对应布防状态（当前驱动里默认返回夜间）。  

---

## 5.10 STM32 代码深度拆解（函数级流程）

> 本节按 `stm32/Project.uvprojx` 的真实编译清单来讲，也就是 `User + BSP + Start + Library` 这条主线。

### 5.10.1 入口调用链（上电后到底先跑谁）

```text
Reset -> startup_stm32f10x_md.s
      -> SystemInit()
      -> main()                    [User/main.c]
      -> HomeDevice_AppInit()      [BSP/home_device_app.c]
      -> while(1) HomeDevice_AppLoop()

中断并行:
SysTick_Handler()   -> HomeDevice_Tick1ms() -> Drivers_TickInc()
USART1_IRQHandler() -> UART1 RX ring buffer 入队
```

一句话理解：主循环是“合作式调度”，中断只做“快速记账/收字节”，重活都在 `HomeDevice_AppLoop()`。

### 5.10.2 `HomeDevice_AppInit()` 初始化顺序和原因

源码顺序是：

1. `Drivers_Init()`：先把 GPIO/ADC/UART/OLED/SysTick 都拉起来。  
2. `init_state()`：清空 `g_sensors/g_state`，给默认模式与阈值。  
3. `Sensor_Init()`、`KeyInput_Init()`、`OledUi_Init()`：业务模块准备。  
4. `CommUart_Init(on_frame_received)`：注册串口收帧回调。  
5. `Air780E_Init()`：给短信模块发 AT 初始化指令。  
6. `AlarmLogic_Init(&g_alarm_ctx)`：清空告警上下文（上次告警位、烟雾累计计数）。  
7. `Scheduler_Init(Drivers_GetMs())`：以当前毫秒数为起点建立各任务计时基线。  

为什么这个顺序比较稳：

1. 先驱动、后业务，避免模块初始化时访问未就绪外设。  
2. 先注册 UART 回调，再进入主循环，防止早期帧丢处理路径。  
3. 调度器最后初始化，可保证周期起点一致。  

### 5.10.3 `HomeDevice_AppLoop()` 一次循环到底做了什么

核心伪代码（与源码一致）：

```c
now = Drivers_GetMs();
CommUart_Poll();                    // 先吃完 UART1 收到的字节，必要时触发 on_frame_received
tasks = Scheduler_GetDueTasks(now); // 得到本轮到期任务位

if (tasks & TASK_KEY)    handle_keys();
if (tasks & TASK_SENSOR) { Sensor_ReadFast(); 烟雾滤波; 每2秒读一次DHT; eval_logic(); }
if (tasks & TASK_OLED)   OledUi_Show(...);
if (tasks & TASK_UPLOAD) send_status_frame();
```

重要点：

1. 串口收帧放在最前面，所以云端命令能尽量快生效。  
2. `TASK_SENSOR` 执行时会同时完成“采样 + 判警 + 执行输出”，这是控制闭环核心。  
3. 上传状态和 OLED 刷新是解耦的，显示卡顿不会阻塞上传。  

### 5.10.4 调度器时间模型（无 RTOS 版）

`scheduler.c` 用 4 个“上次执行时刻”做差值判断：

1. `TASK_KEY`：20ms  
2. `TASK_SENSOR`：200ms  
3. `TASK_OLED`：500ms  
4. `TASK_UPLOAD`：1000ms  

因此系统节拍关系可以记成：

1. 按键 50Hz  
2. 传感采样 5Hz  
3. 屏幕刷新 2Hz  
4. 云上传 1Hz  

### 5.10.5 关键全局变量（建议调试时重点观察）

`home_device_app.c` 里这些变量最关键：

1. `g_sensors`：当前传感器快照（温湿度/烟雾/火焰/入侵）。  
2. `g_state`：模式、阈值、继电器、蜂鸣器、OLED 页。  
3. `g_alarm_ctx`：告警算法上下文（上一轮标志、烟雾累计次数）。  
4. `g_alarm_flags`：本轮告警位（烟雾/火焰/入侵）。  
5. `g_silence_latch`：静音锁存位。  
6. `g_manual_relay_enable` + `g_manual_relay_value`：手动继电器控制锁存。  
7. `g_smoke_hist[8]` + `g_smoke_sum`：烟雾滑动平均滤波窗口。  

建议在 Keil Watch 窗口先盯这 7 组变量，能覆盖 80% 现场问题。

### 5.10.6 采样与滤波细节（为什么烟雾不“乱跳”）

1. 快速采样 `Sensor_ReadFast()`：每 200ms 读一次 MQ2/火焰/人体。  
2. 慢速采样 `Sensor_ReadSlow()`：`g_dht_divider` 每满 `DHT_SAMPLE_DIV=10` 才读一次温湿度，即约每 2 秒。  
3. 烟雾值先做 8 点均值滤波：`filtered = sum(last8) / 8`。  

结论：上传到云端和 OLED 的烟雾值是“平滑后”的值，抗抖比原始 ADC 好。

### 5.10.7 告警判定与优先级（核心状态机）

`AlarmLogic_Update()` 规则：

1. 烟雾：`smoke >= threshold` 连续计数，达到 `SMOKE_OVER_COUNT_TO_ALARM=5` 触发。  
2. 火焰：`sensors->flame == 1` 立即触发。  
3. 入侵：仅 `MODE_ARM` 时 `intrusion==1` 才触发。  
4. `rising_flags = current & ~prev`，只在上升沿触发短信和告警帧。  

按当前周期换算，烟雾要“连续超阈值约 1 秒”才报警（`200ms * 5`）。

### 5.10.8 输出决策链（继电器/蜂鸣器最终谁说了算）

`apply_outputs()` 的优先级是固定的：

1. 若烟雾或火焰告警，继电器强制开（喷淋优先）。  
2. 否则若手动继电器使能，按手动值输出。  
3. 否则继电器关。  
4. 任意告警默认蜂鸣器开。  
5. 若 `g_silence_latch=1`，蜂鸣器强制静音。  

还有一个细节：出现“新的告警上升沿”时，代码会自动清除静音锁存，让新告警重新发声。

### 5.10.9 本地按键与云端命令如何映射到代码

本地按键在 `handle_keys()`：

1. `K1`：换页 `0 -> 1 -> 2 -> 0`。  
2. `K2`：模式循环 `DISARM -> ARM -> AUTO -> DISARM`。  
3. `K3`：在阈值页时阈值 `+10`，其他页为手动开继电器。  
4. `K4`：在阈值页时阈值 `-10`，其他页为静音。  

云端命令在 `on_frame_received()`：

1. `PROTO_TYPE_CTRL`：`SET_MODE / SET_RELAY / SILENCE`。  
2. `PROTO_TYPE_PARAM`：`SET_SMOKE_TH`。  
3. 每条合法命令都会 `send_ack(command_id)` 回 ACK 给 ESP32。  

### 5.10.10 STM32<->ESP32 帧载荷定义（代码实值）

`send_status_frame()` 的 ENV 负载固定 10 字节：

1. `payload[0]`：温度（int8 转 uint8 发送）  
2. `payload[1]`：湿度  
3. `payload[2]`：烟雾低字节  
4. `payload[3]`：烟雾高字节  
5. `payload[4]`：火焰  
6. `payload[5]`：入侵  
7. `payload[6]`：模式  
8. `payload[7]`：继电器  
9. `payload[8]`：蜂鸣器  
10. `payload[9]`：Air780E 状态位  

告警帧 `PROTO_TYPE_ALARM`：

1. `data[0]`：告警类型（1 烟雾，2 火焰，3 入侵）  
2. `data[1]`：告警等级  
3. 后续：ASCII 告警文本（如 `smoke warning`）  

### 5.10.11 中断与缓冲机制（避免“看起来卡死”）

1. `SysTick_Handler()` 只做 `HomeDevice_Tick1ms()`，非常轻。  
2. `USART1_IRQHandler()` 只把字节塞进 128 字节环形缓冲区。  
3. 若缓冲区满，新字节会被丢弃（代码通过 `next != tail` 判断）。  
4. 真正的协议解析在主循环 `CommUart_Poll()` 里完成。  

这套设计的意义：中断短小、主循环可控，便于调试与定位。

### 5.10.12 目前代码里的“占位实现/注意事项”（非常重要）

1. `Drivers_ReadDht11()` 目前返回固定 `25C/50%`（占位实现），不是实测温湿度。  
2. `Drivers_IsNightTime()` 目前恒返回 `1`，所以 `AUTO` 现阶段等价于“总是按夜间逻辑”。  
3. `g_state.net_ok`、`g_state.air780e_ok` 目前初始化为 1，但未基于真实链路回读更新。  
4. `Air780E_SendSms()` 采用“直接发 AT + Ctrl+Z”，当前没有解析回包做成功/失败确认。  

也就是说：演示链路能跑通，但若做量产/长期稳定，需要补“回包状态机 + 超时重试 + 真实 DHT 驱动 + RTC 时段逻辑”。

### 5.10.13 新手调试 STM32 的最短路径（建议照这个顺序）

1. 先断开 ESP32，只看 STM32：按键是否能切页、改模式、改阈值。  
2. 再接 ESP32，只看串口：是否持续有 `PROTO_TYPE_ENV` 帧发出。  
3. 再测下行命令：发送 `SET_RELAY`，看 `send_ack()` 是否执行。  
4. 再测告警：人为拉高烟雾输入，观察 `g_alarm_flags` 与继电器/蜂鸣器动作。  
5. 最后测短信：仅在告警上升沿会触发 `Air780E_SendSms()`。  

---

## 6. 硬件连接总表（按当前代码）

## 6.1 STM32 <-> ESP32

| 功能 | STM32F103 | ESP32-C3 | 说明 |
|---|---|---|---|
| 串口发送到 ESP | PA9 (USART1_TX) | GPIO6 (UART1_RX) | 交叉连接 |
| 串口接收自 ESP | PA10 (USART1_RX) | GPIO7 (UART1_TX) | 交叉连接 |
| 地 | GND | GND | 必须共地 |

## 6.2 STM32 <-> Air780E

| 功能 | STM32F103 | Air780E | 说明 |
|---|---|---|---|
| AT发送 | PA2 (USART2_TX) | RXD | 交叉连接 |
| AT接收 | PA3 (USART2_RX) | TXD | 交叉连接 |
| 地 | GND | GND | 必须共地 |

## 6.3 STM32 本地外设

| 外设 | STM32 引脚 | 方向 |
|---|---|---|
| MQ2 模拟量 | PA0 (ADC1_CH0) | 输入 |
| DHT11 数据 | PA8 | 输入 |
| 火焰传感器 | PB12 | 输入 |
| 人体红外 | PB13 | 输入 |
| OLED SCL | PB8 | 输出 |
| OLED SDA | PB9 | 输出 |
| 蜂鸣器 | PB0 | 输出 |
| 继电器 | PB1 | 输出 |
| KEY1 | PB14 | 输入 |
| KEY2 | PB15 | 输入 |
| KEY3 | PA6 | 输入 |
| KEY4 | PA7 | 输入 |

## 6.4 供电重要提醒（非常关键）

1. 全系统共地：STM32/ESP32/Air780E/传感器 GND 必须共地。  
2. Air780E 不能直接吃 STM32 3.3V，需稳定独立电源（发射时电流脉冲大）。  
3. 串口电平需匹配模块规格（必要时加电平转换）。

---

## 7. 串口与网络参数

## 7.1 串口参数

| 设备 | 端口 | 波特率 | 格式 |
|---|---|---|---|
| STM32<->ESP32 | USART1 | 115200 | 8N1 |
| STM32<->Air780E | USART2 | 115200 | 8N1 |
| ESP32 下载日志 | USB CDC/UART | 115200 | 8N1 |

## 7.2 网络参数

- ESP32 Wi-Fi：在 `ESP32/main/main.c` 里配置 `WIFI_SSID` / `WIFI_PASS`。  
- 服务器地址：
  - ESP32 使用 `SERVER_BASE`。
  - 手机端默认在 `HbuilderX/common/config.js` 的 `DEFAULT_BASE_URL`。

---

## 8. 后端 API 一览（给联调用）

| 方法 | 路径 | 调用方 | 作用 |
|---|---|---|---|
| POST | `/api/device/upload` | ESP32 | 上传状态/告警/ACK |
| GET | `/api/device/cmd?device_id=...` | ESP32 | 拉待执行命令 |
| POST | `/api/app/login` | APP | 登录 |
| GET | `/api/app/status?device_id=...` | APP | 查实时状态 |
| POST | `/api/app/control` | APP | 下发控制命令 |
| GET | `/api/app/history` | APP | 查历史告警 |
| WS | `/ws/alarm` | APP | 收状态/告警推送 |

---

## 9. 从零部署（一步一步）

> 推荐顺序：先跑 Server，再烧 STM32，再烧 ESP32，最后接线联调 APP。

## 9.1 准备软件

1. **Keil MDK5**（编 STM32）  
2. **Python 3.10+**（跑后端）  
3. **ESP-IDF v5.3.1**（编 ESP32）  
4. **HBuilderX**（跑 uni-app）  
5. **串口工具**（看日志/发 AT）

## 9.2 启动 Server

在项目根目录执行：

```powershell
cd Server
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload
```

验证：

- 浏览器打开 `http://<服务器IP>:8000/health`，应返回 `{"ok": true}`。

默认账号：

- 用户名：`admin`
- 密码：`123456`

## 9.3 编译/烧录 STM32（Keil）

1. 用 Keil 打开 `stm32/Project.uvprojx`。  
2. 选择目标芯片（工程已配置 F103C8）。  
3. 点击 Build（锤子图标）。  
4. 连接下载器（ST-LINK/J-LINK）。  
5. Download 烧录。  
6. 确认程序运行（OLED/串口有活动）。

## 9.4 编译/烧录 ESP32（ESP-IDF）

```powershell
cd ESP32
idf.py set-target esp32c3
idf.py build
idf.py -p COM16 flash monitor
```

> 把 `COM16` 换成你的实际端口。

在 `main/main.c` 先改：

- `WIFI_SSID`
- `WIFI_PASS`
- `SERVER_BASE`
- `DEVICE_ID`

## 9.5 连接硬件线

按“第 6 章连接表”逐条接线，重点检查：

1. STM32<->ESP32 UART 是否 TX/RX 交叉。  
2. STM32<->Air780E UART 是否 TX/RX 交叉。  
3. 是否全系统共地。  
4. Air780E 电源是否单独稳定。  
5. OLED 是否接在 `PB8/PB9`（不是旧文档的 `PB10/PB11`）。

## 9.6 手机端配置

1. 用 HBuilderX 打开 `HbuilderX/`。  
2. 运行到模拟器或真机。  
3. 进入“设置”页，填写：
   - 后端地址（例如 `http://42.192.113.88:8000`）
   - 设备 ID（默认 `HD-001`）
4. 点击测试连接。

## 9.7 首次联调检查清单

1. Server 控制台是否有 ESP32 上传请求日志。  
2. ESP32 日志是否显示 Wi-Fi 已拿到 IP。  
3. ESP32 `diag` 是否由 `valid=0` 变为 `valid=1`。  
4. APP 实时页是否能看到温湿度/烟雾变化。  
5. APP 控制页下发命令后，设备是否执行。

---

## 10. 项目运行时序（启动后发生什么）

## 10.1 STM32 启动

1. `User/main.c` 进入 `HomeDevice_AppInit()`。  
2. 初始化驱动、传感器、按键、OLED、UART、短信模块。  
3. 启动 1ms 系统节拍。  
4. 主循环按调度器执行任务。  
5. 每 1 秒向 ESP32 发送一次状态帧。

## 10.2 ESP32 启动

1. 初始化 NVS、UART、Wi-Fi。  
2. 进入主循环。  
3. 持续解析 STM32 串口帧。  
4. 每秒 `post_upload()` 上传状态。  
5. 每秒 `poll_command()` 拉控制命令。

## 10.3 Server 运行

1. 启动时自动建表 + 默认账号/默认设备。  
2. 接受设备上传并更新状态。  
3. 若有告警则写历史并 WS 广播。  
4. 接受 APP 控制并排队命令。

## 10.4 APP 运行

1. 实时页每 2 秒轮询状态。  
2. 同时连接 WebSocket 接收推送。  
3. 控制页发命令到 `/api/app/control`。  
4. 历史页读 `/api/app/history`。

---

## 11. 协议细节

## 11.1 UART 帧协议（STM32 <-> ESP32）

格式：

```text
AA 55 TYPE LEN DATA... CHECK
```

- `TYPE`：
  - `0x01` 环境状态
  - `0x02` 告警
  - `0x03` 控制
  - `0x04` 参数
  - `0x05` ACK
- `CHECK`：`(TYPE + LEN + sum(DATA)) & 0xFF`

## 11.2 设备上传 JSON（ESP32 -> Server）

核心字段：

- `device_id`
- `temperature/humidity/smoke/flame/intrusion`
- `mode/relay/buzzer/network/air780e`
- 可选：`alarm_type/alarm_level/alarm_message`
- 可选：`ack_command_id`

## 11.3 短信 AT 逻辑（STM32 -> Air780E）

初始化：

- `AT`
- `AT+CPIN?`
- `AT+CSQ`
- `AT+CMGF=1`
- `AT+CSCS="GSM"`

发送短信：

- `AT+CMGF=1`
- `AT+CMGS="手机号"`
- `正文`
- `0x1A`（Ctrl+Z）

---

## 12. 常见问题排查（最实用）

## 12.1 ESP32 一直 `valid=0`

现象：Wi-Fi 连上了，但上传一直失败计数增加。  
原因：ESP32 还没收到 STM32 的首帧（UART 未通/STM32未发）。

检查顺序：

1. STM32 是否在跑主循环。  
2. STM32->ESP32 串口线是否 TX/RX 交叉。  
3. 共地是否可靠。  
4. 波特率是否同为 115200。  
5. ESP32 日志里 `rx_bytes` 是否增长。

## 12.2 串口工具发短信 `ERROR`

常见误区：输入了文本 `0x1A`，而不是真实控制字符。  
正确做法：发送真实字节 `1A` 或按 `Ctrl+Z`（如果工具支持）。

## 12.3 Air780E 有网但发不出短信

检查：

1. SIM 是否开通短信能力。  
2. 天线是否接好。  
3. 电源是否稳。  
4. 是否使用 GSM 字符集发中文（会失败）。先用英文验证。

## 12.4 手机端连不上后端

1. 手机与服务器网络是否互通。  
2. `baseUrl` 是否写成可访问 IP（不是手机本地 127.0.0.1）。  
3. 防火墙/安全组是否放行 `8000`。  
4. 后端是否正在运行。

## 12.5 ESP32 日志提示 Flash 4MB/2MB 不一致

你日志里出现过：芯片检测 4MB，镜像头是 2MB。  
不是立刻致命，但建议后续统一 `sdkconfig` 的 Flash 大小，避免分区浪费或误判。

---

## 13. 推荐验证脚本（建议照做）

## 13.1 后端健康检查

```powershell
curl http://127.0.0.1:8000/health
```

预期：`{"ok":true}`。

## 13.2 设备上行链路

- 看 ESP32 日志：`wifi=1 valid=1 upload_ok` 应持续增长。

## 13.3 命令下行链路

1. APP 下发 `SET_RELAY=1`。  
2. ESP32 日志应出现命令轮询成功。  
3. STM32 执行后回 ACK。  
4. Server 中对应命令状态应到 `DONE`。

## 13.4 告警链路

1. 触发烟雾/火焰/入侵条件。  
2. APP 实时页应收到 WS 告警。  
3. 历史页应出现新记录。  
4. Air780E 发送短信（若硬件/网络正常）。

---

## 14. 开发者给新手的实话建议

1. 先保证“串口通”，再谈“网络通”。  
2. 先让英文短信成功，再做中文编码。  
3. 每改一处只验证一个链路，别一次改太多。  
4. 日志是第一诊断工具：STM32 串口日志 + ESP32 日志 + Server 日志同时看。

---

## 15. 你可以先改的配置清单（最常用）

### STM32

- `stm32/BSP/app_config.h`
  - `APP_PHONE_NUMBER`
  - `DEFAULT_SMOKE_THRESHOLD`
  - 任务周期参数

### ESP32

- `ESP32/main/main.c`
  - `WIFI_SSID`
  - `WIFI_PASS`
  - `SERVER_BASE`
  - `DEVICE_ID`

### APP

- `HbuilderX/common/config.js`
  - `DEFAULT_BASE_URL`
  - `DEFAULT_DEVICE_ID`

### Server

- `Server/app/core/config.py`
  - 默认账号/密码
  - 默认设备 ID

---

## 16. 版本与环境（当前仓库可见）

- ESP-IDF：`v5.3.1`（日志可见）  
- Python 依赖：
  - `fastapi==0.116.1`
  - `uvicorn[standard]==0.35.0`
  - `pydantic==2.11.7`
- STM32 编译器（日志）：ARMCC V5.06（Keil）

---

## 17. 附：最简“上电必看”清单

1. 接线：UART 交叉 + 共地。  
2. Server 已启动且可访问。  
3. ESP32 Wi-Fi 与 Server 地址已改对。  
4. STM32 手机号已改对。  
5. Air780E 供电稳定、SIM 正常。  
6. ESP32 日志出现 `valid=1`。

做到这 6 条，项目大概率一次跑通。

