# V3 蓝牙 HCI RX / PER 专项测试说明

本文档从仓库脚本 `V3蓝牙PER专项验证_Rev1.0.cs` 中提取，对应 **DUT HCI 非信令 RX 收包** 与 **CMW100 GPRF ARB 发包** 的 BLE PER 专项验证流程。

---

## 1. 脚本定位


| 项目  | 说明                                                                                                     |
| --- | ------------------------------------------------------------------------------------------------------ |
| 源文件 | `V3蓝牙PER专项验证_Rev1.0.cs`                                                                                |
| 用途  | 仅保留 BLE Shell、可选产测准备、**DUT HCI RX**、CMW100 GPRF ARB 发包；在 2402 / 2440 / 2480 MHz（可配置）上验证 **BLE RX PER** |
| 不包含 | MES、写 SN、三键应用、显示测试、complete、API 上报等产线全流程                                                               |


---

## 2. 测试主流程（与 RX 相关）

1. **BLE Shell 连接**（`V3_Exe.bat` + 连接模式）
2. 可选 **进入产测**（`enter`）
3. **进入非信令模式**（默认 Shell 命令 `bt-nonsig-on`，参数 `BlePer_NonSignalingShellCommand`）
4. 若 `BlePer_CloseShellBeforeHci` 为 true：**关闭 BLE Shell**，再经延时切换 **HCI UART**
5. **CMW100 VISA** 连接并初始化 **GPRF ARB**
6. 打开 **HCI 串口**，发送可选 UART 初始化 HEX
7. **三频点（或多场景）PER**：仪表发包 → DUT 通过 HCI 读收包数
8. UART 退出 HEX、清理资源

---

## 3. 单场景 RX / PER 逻辑（核心）

对每个频点 + PHY 场景，顺序为：

1. **HCI Reset**
  - 默认 HEX：`01030C00`（参数 `BlePer_HciResetHex`）  
  - 应答校验片段：`030C00`（可关 `BlePer_VerifyHciResponse`）
2. **Start RX（非信令收包）**
  - 由 `BuildBlePerRxStartHex` 构造：  
   `01 33 20 03 | ChannelIndex | PhyCode | 00`  
  - 即 HCI **LE Receiver Test** 类命令封装（`0x33`、`0x20` 为脚本中固定操作码/长度字段）  
  - 应答校验片段：`332000`
3. **CMW100 GPRF ARB 发包**
  - 切频：`SOURce:GPRF:GEN:RFSettings:FREQuency {MHz}MHz`  
  - 触发：`TRIGger:GPRF:GEN:ARB:MANual:EXECute`  
  - 发包周期数等与 `BlePer_CmwArbCycles`、`BlePer_CmwWaitArbScount` 等参数相关
4. **HCI Test End**
  - 默认 HEX：`011F2000`（参数 `BlePer_HciTestEndHex`）  
  - 应答校验片段：`1F2000`
5. **解析 RxCount 与 PER**
  - `ParseBlePerRxCount`：取应答 **最后 2 字节** 为小端 **16 位收包数**  
  - `PER = (BlePer_TxCount - rxCount) / BlePer_TxCount * 100%`  
  - 判定：`per <= BlePer_MaxPercent`（默认 30.8%），且 `BlePer_TxCount > 0`、`rxCount >= 0`

---

## 4. 频点与信道映射

- 场景列表参数：`BlePer_ScenarioList`，默认 `2402:1M,2440:1M,2480:1M`  
- 支持别名：`LOW`→2402、`MID`→2440、`HIGH`→2480  
- PHY：`1M`→`0x01`，`2M`→`0x02`  
- `FrequencyToBlePerChannel`：  
  - 2402 MHz → 信道索引 **0**  
  - 2440 MHz → **19**  
  - 2480 MHz → **39**  
  - 其它 2402–2480 MHz：`freqMHz - 2402`

---

## 5. 主要可配置参数（RX/PER 相关摘录）


| 参数名                                           | 默认/说明                                          |
| --------------------------------------------- | ---------------------------------------------- |
| `BlePer_ScenarioList`                         | 频点:PHY 列表                                      |
| `BlePer_TxCount`                              | 期望发包数（算 PER 的分母），默认 1000                       |
| `BlePer_MaxPercent`                           | PER 上限，默认 30.8                                 |
| `BlePer_MaxAttempts` / `BlePer_RetestCount`   | 单场景最大尝试次数                                      |
| `BlePer_UartPort` / `BlePer_UartBaudRate`     | HCI 串口                                         |
| `BlePer_HciResetHex` / `BlePer_HciTestEndHex` | HCI 命令                                         |
| `BlePer_NonSignalingShellCommand`             | 非信令准备 Shell 命令（未配置且要求必发时会报错，禁止直接进入 HCI RX）     |
| `BlePer_CloseShellBeforeHci`                  | Shell 与 HCI 切换前是否关闭 BLE 连接                     |
| `BlePer_CmwVisaAddress`                       | CMW100 VISA 地址                                 |
| `BlePer_CmwWaveformFile`                      | ARB 波形，如 `@WAVEFORM\BT_LE_TestPacket_P9_1M.wv` |
| `BlePer_CmwArbCycles`                         | 与仪表 ARB cycles 对齐，默认与 `BlePer_TxCount` 相同      |
| `BlePer_CmwEnableFixedInit`                   | 是否在脚本内做完整 GPRF 固定初始化（false 时仅清错并依赖后续切频+手动触发）   |


完整列表以源脚本 **Parameters** 区段为准。

---

## 6. 日志关键字

- `BLE_PER_UART_TX` / `BLE_PER_UART_RX`：HCI 串口收发 HEX  
- `BLE PER`：场景进度、RxCount、PER、复测信息  
- `CMW100`：仪表连接、ARB 触发与错误查询

---

## 7. 源码位置索引


| 内容                                  | 源文件中大致位置      |
| ----------------------------------- | ------------- |
| 文件头说明与参数                            | 文件开头 ~ L87    |
| `MainTest` 主步骤                      | ~ L91–L172    |
| 非信令进入 `EnterBlePerNonSignalingMode` | ~ L366–L397   |
| 单场景 RX `RunBlePerScenarioAttempt`   | ~ L602–L661   |
| `BuildBlePerRxStartHex` / 信道解析      | ~ L664–L726   |
| CMW GPRF ARB                        | ~ L728 起      |
| UART 发送与 `ParseBlePerRxCount`       | ~ L1007–L1085 |


---

*文档由脚本内容整理生成，与仪器/固件版本强相关；现场以实际参数与 HCI 规范为准。*

---

## 8. 当前覆盖频点与所用指令（脚本默认）

以下内容依据 `V3蓝牙PER专项验证_Rev1.0.cs` 的**默认参数**整理；若工装上已修改 `BlePer_ScenarioList`、`BlePer_NonSignalingShellCommand`、`BlePer_HciResetHex` 等，以**实际下发参数**为准。

### 8.1 已覆盖频点（默认 `BlePer_ScenarioList`）

| 顺序 | 频点 (MHz) | BLE 信道索引 | PHY |
| --- | --- | --- | --- |
| 1 | 2402 | 0 | 1M |
| 2 | 2440 | 19 | 1M |
| 3 | 2480 | 39 | 1M |

即 **BLE 2.4 GHz 低 / 中 / 高三频点**、**LE 1M** 各跑一轮 PER；未在默认列表中包含 2M 或其它中间信道（除非改参数）。

### 8.2 指令清单（按链路分类）

**BLE Shell（经 `V3_Exe.bat`）**

| 阶段 | 默认内容 | 说明 |
| --- | --- | --- |
| 产测准备 | `enter` | `BLE_EnterTestMode` 为 true 时发送 |
| 非信令 | `bt-nonsig-on`（默认） | `BlePer_NonSignalingShellCommand` 未显式配置时：若 `BlePer_PrepareShellCommand` 非空则用其内容，否则为 `bt-nonsig-on`；亦可直接配置 `BlePer_NonSignalingShellCommand` 覆盖 |

**DUT HCI（UART HEX，每频点一轮 RX 流程）**

| 步骤 | 默认 HEX / 格式 | 参数名（可覆盖） |
| --- | --- | --- |
| HCI Reset | `01030C00` | `BlePer_HciResetHex` |
| LE Receiver Test（Start RX） | `01 33 20 03 \| Channel \| Phy \| 00`（脚本 `BuildBlePerRxStartHex`） | 操作码固定于脚本；Channel/Phy 由频点与 PHY 推导 |
| HCI Test End | `011F2000` | `BlePer_HciTestEndHex` |

可选：`BlePer_UartInitCommandsHex`、`BlePer_UartExitCommandsHex`（默认空）在打开/关闭串口阶段追加发送。

**CMW100（GPRF ARB，每频点场景）**

| 典型指令 | 说明 |
| --- | --- |
| `*CLS` | 清状态，场景开始前 |
| `SOURce:GPRF:GEN:RFSettings:FREQuency {频点}MHz` | 切到当前 BLE 频点 |
| `TRIGger:GPRF:GEN:ARB:MANual:EXECute` | 手动触发 ARB 发包 |
| `SYSTem:ERRor?` | 场景后查错（`BlePer_CmwCheckErrorAfterScenario` 为 true 时） |

连接仪表时还会执行 `*IDN?`。若 `BlePer_CmwEnableFixedInit` 为 true，初始化阶段另含 `SOURce:GPRF:GEN:BBMode ARB`、`ARB:CYCLes`、`ARB:REPetition`、`RFSettings:LEVel`、`STATe ON`、`ARB:FILE` 等（见脚本 `InitializeBlePerCmwGprfArb`）。若 `BlePer_CmwUseGuiRfConfig` 为 false，单场景内可能再发 `ARB:REPetition`、`ARB:CYCLes`。若 `BlePer_CmwWaitArbScount` 为 true，会轮询 `SOURce:GPRF:GEN:ARB:SCOunt?` 直至周期完成。