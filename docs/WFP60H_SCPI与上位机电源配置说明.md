# WFP60H 双通道电池模拟器：SCPI 与上位机电源配置说明

**设备标识**：会凌电子（品牌标识因曼吉），英文 **HUILING ELECTRONIC**；型号 **WFP60H**。  
重构后 device 目录：**`scpi/device/huiling_wfp60h_scpi/`**（SCPI 设备归 **scpi 域**，不归 modbus；见 [agreement按外设域拆分重构方案.md](./agreement按外设域拆分重构方案.md) §3.3、§3.4）。

本文档根据仓库内 **`WFP60H双通道电池通讯说明.docx`**（Word 内 `word/document.xml` 文本抽取）中与 **设置输出电压/电流**、**测量电压/电流** 相关的 **6.3 节命令树**整理，并说明与本项目 **`上位机设置.ini`**、`Qusb`、`吸力工站 suction` 的对应关系。

> **说明**：`[]`、`[:]` 为 SCPI 可选关键字；编程时可采用**长形**或仪器支持的**短形**（文档亦说明方括号/尖括号在编程时的省略规则）。以下「推荐写入 ini 的短形」需在实机上以 `*IDN?`、`SYST:ERR?` 做最终确认。

---

## 一、输出开关（程控输出 ON/OFF）

| 文档小节（抽取） | 写 / 查 |
|------------------|---------|
| 6.3.1.1 | `OUTPut[1][:STATe]` / `OUTPut[1][:STATe]?` |
| 6.3.1.2 | `OUTPut2[:STATe]` / `OUTPut2[:STATe]?` |
| 另有双通道同时类关键字（抽取中出现） | `:BOTHOUTON`、`:BOTHOUTOFF` |

**ini 对应**：`VisaPower/ScpiOutputOnCmd`、`ScpiOutputOffCmd`（吸力优先读 `[VisaPower]`，见 `work_station/suction/suction.cpp` 中 `applySuctionProtocolConfig`）。

**示例（通道 1，需实机验证）**：

```ini
ScpiOutputOnCmd=OUTPut1:STATe ON
ScpiOutputOffCmd=OUTPut1:STATe OFF
```

---

## 二、设置输出电压（SOURCE）

| 文档小节 | 命令（设） | 命令（查） |
|----------|------------|------------|
| 6.3.2.1 通道 1 | `[SOURce[1]]:VOLTage[:LEVel][:IMMediate][:AMPLitude] <n>` | `[SOURce[1]]:VOLTage[:LEVel][:IMMediate][:AMPLitude]?` |
| 6.3.2.2 通道 2 | `SOURce2:VOLTage[:LEVel][:IMMediate][:AMPLitude] <n>` | `SOURce2:VOLTage[:LEVel][:IMMediate][:AMPLitude]?` |

电压范围以说明书正文为准（抽取文本中可见 **0~20 V** 等与面板相关的描述；编程以厂家标定为准）。

**ini 对应**：`VisaPower/ScpiSetVoltageCmd`、`ScpiReadVoltageCmd`  
模板中 **`%1`** 由程序替换为数值（`QString::arg`，见 `agreement/qusb/qusb.cpp` 的 `configureProgrammablePower`）。

**推荐示例（通道 1）**：

```ini
ScpiSetVoltageCmd=SOURce1:VOLTage:LEVel:IMMediate:AMPLitude %1
ScpiReadVoltageCmd=SOURce1:VOLTage:LEVel:IMMediate:AMPLitude?
```

---

## 三、设置输出电流限（SOURCE 电流限制）

| 文档小节 | 命令（设） | 命令（查） |
|----------|------------|------------|
| 6.3.2.7 通道 1 | `[SOURce[1]]:CURRent[:LIMit][:VALue] <n>` | `[SOURce[1]]:CURRent[:LIMit][:VALue]?` |
| 6.3.2.8 通道 2 | `SOURce2:CURRent[:LIMit][:VALue] <n>` | `SOURce2:CURRent[:LIMit][:VALue]?` |

**ini 对应**：`VisaPower/ScpiSetCurrentCmd`、`ScpiReadCurrentCmd`（注意：工程里该键名表示「限流设定/查询」，与下节「测量电流」不同）。

**推荐示例（通道 1）**：

```ini
ScpiSetCurrentCmd=SOURce1:CURRent:LIMit:VALue %1
ScpiReadCurrentCmd=SOURce1:CURRent:LIMit:VALue?
```

---

## 四、测量电压 / 电流（与默认 `MEASure:` 的差异）

文档抽取中 **6.3.4** 一类命令与 **`SENSe`** 配置、**`MEASure` / `READ` / `FETCh`** 族相关，包括但不限于：

| 类型 | 抽取中出现的典型形式（通道 1 / 2） |
|------|--------------------------------------|
| 读一次平均 | `READ[1]?`、`READ2?`、`BOTHREAD?` |
| 测量 | `MEASure[1]?`、`MEASure2?`、`MEASure[1]:ARRay?` 等 |
| 取数 | `FETCh[1]?`、`FETCh2?`、`BOTHFETCH?` |
| 测量功能 / 量程等 | `SENSe[1]:FUNCtion`、`SENSe[1]:CURRent[:DC]:RANGe[:UPPer]` 等 |

抽取文本中说明：`MEASure[1]?` 等返回与当前测量功能相关；**电压**与**电流**在说明中对应 **`VOLTage[:DC]`**、**`CURRent[:DC]`** 语义（见抽取段 6.3.4.6 / 6.3.4.7 附近对 `VOLTage[:DC]`、`CURRent[:DC]` 的区分描述）。

**与本项目默认 ini 的差异**：

- 当前仓库常见默认：`MEASure:VOLTage:DC?`、`MEASure:CURRent:DC? ...`（万用表式写法）。
- WFP60H 通讯说明 **6.3.4.6 / 6.3.4.7**：应使用 **`MEASure[1]:<function>?`** 形式，在查询中带 **`VOLTage[:DC]`** 或 **`CURRent[:DC]`**（或 **`DVMeter`**），仪器会先切测量功能再读数；**裸 `MEASure1?` / `READ1?`** 依赖「当前功能」，上位机无法从串本身区分电压/电流。

**建议**：`ScpiReadVoltageCmd` / `ScpiReadCurrentCmd` 分别配置 **`MEASure1:VOLTage:DC?`** 与 **`MEASure1:CURRent:DC?`**（通道 2 用 **`MEASure2:...`**）。

**ini 示例**：

```ini
; 与通讯说明 6.3.4.6 一致，语义明确
ScpiReadVoltageCmd=MEASure1:VOLTage:DC?
ScpiReadCurrentCmd=MEASure1:CURRent:DC?
```

---

## 五、上位机工程中的逻辑关系（简要）

1. **`上位机设置.ini`**
   - **`[VisaPower]`**：**全工站共用**的 **程控电源（WFP60H 等）VISA、地址、电压/限流、全部电源 SCPI 模板**；吸力 **`powerProtocolConfig_`** 与 **`Qusb::programmablePowerDefaultsWfp60h()`** 中的 VISA 项**仅读本节**（键缺失时用代码内 WFP 默认串作兜底）。**不再使用 `[ProgrammablePower]`**。
   - **`[Suction]`**：**传感器/测量**（协议、DAM、阈值及 **usb 万用表** 用 `Scpi*` 等）；万用表式指令可经 `Suction/*` 覆盖后再回退 **`VisaPower/*`**。

2. **`[Current]`（静态电流工站）**  
   当 **`Current/UseProgrammablePower=true`** 时，未单独配置的 **`Current/Scpi*`** 回退为 **`VisaPower/*`**，再不足则用 **`Qusb::programmablePowerDefaultsWfp60h()`** 内嵌默认；为 **false** 时仍可用万用表式 **`VOLT %1` / `MEASure:CURRent:DC?`** 等字面默认。

3. **`suction::applySuctionProtocolConfig()`**（`work_station/suction/suction.cpp`）  
   仅将 **`[VisaPower]`** 读入 **`powerProtocolConfig_`**（电源侧固定 **SCPI**）。

4. **`Qusb::configureProgrammablePower`**（`agreement/qusb/qusb.cpp`）  
   依次 **`sendCmd(ScpiSetVoltageCmd.arg(...))`**、**`sendCmd(ScpiSetCurrentCmd.arg(...))`**。

5. **`Qusb::setProgrammablePowerOutput`**  
   发送 **`ScpiOutputOnCmd` / `ScpiOutputOffCmd`**。

6. **读测量**  
   **`getMEASure` / `readProgrammablePowerCurrent` / `readProgrammablePowerVoltage`** 使用 **`ScpiReadCurrentCmd`** 或 **`ScpiReadVoltageCmd`**（VISA 时经 **`visaWrite` / `visaQuery`**）。

7. **BYD 分支**  
   **`handlebydAction(ConfigurePowerSupply)`** 内也会发 **`sendCONF` + 电压/电流 set**；受 **`powerProtocolConfig_`** 中字符串影响。

---

## 六、VISA 资源字符串（文档抽取中的示例）

抽取文本中出现的示例（仅作格式参考，以现场仪器为准）：

- `TCPIP0::192.168.155.233::inst0::INSTR`
- `USB0::0x0734::0x0431::21013001-USB1::INSTR`
- `USB0::0x0734::0x0431::21013001-USB2::INSTR`

对应 ini：**`VisaPower/VisaAddress`**，并设 **`VisaPower/ScpiUseVisa=true`**。

---

## 七、`Qusb::programmablePowerDefaultsWfp60h()` 与 `[VisaPower]` 示例

代码入口：`agreement/qusb/qusb.h` 声明，`qusb.cpp` 实现。默认串为 **WFP60H 通道 1** 长名写法；读数按通讯说明 **6.3.4.6** 使用 **`MEASure1:VOLTage:DC?` / `MEASure1:CURRent:DC?`**。**VISA 开关与地址仅读 `[VisaPower]`**。

```ini
[VisaPower]
ScpiSetVoltageCmd=SOURce1:VOLTage:LEVel:IMMediate:AMPLitude %1
ScpiSetCurrentCmd=SOURce1:CURRent:LIMit:VALue %1
ScpiOutputOnCmd=OUTPut1:STATe ON
ScpiOutputOffCmd=OUTPut1:STATe OFF
ScpiReadVoltageCmd=MEASure1:VOLTage:DC?
ScpiReadCurrentCmd=MEASure1:CURRent:DC?
PowerVoltageV=12.0
PowerCurrentLimitA=2.5
ScpiUseVisa=true
VisaAddress=GPIB0::7::INSTR
```

---

## 八、维护说明

- 本文档 SCPI **小节号与命令拼写**来自对 **`WFP60H双通道电池通讯说明.docx`** 的自动抽取；若与印刷版/修订版 PDF 不一致，**以厂家最新版通讯说明为准**。
- 修改 ini 后需重新运行程序或重新加载配置（吸力界面「程控电源(VISA)」行 **保存并应用** 会写回 `VisaPower` 并 `applySuctionProtocolConfig`）。
