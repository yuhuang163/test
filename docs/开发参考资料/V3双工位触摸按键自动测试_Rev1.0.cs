// ============================================
// V3 双工位触摸按键自动测试脚本
//
// 目标场景：
// 1. 双工位手动扫码
// 2. 每个工位独立连接 BLE Shell
// 3. 上位机通过汇川 PLC 驱动手指移动到固定点位并按压
// 4. 校验设备返回的按键事件
// 5. 读取对应的 key-cap 电容值并判定
//
// 说明：
// - 本脚本按 BYDTEST 现有多通道模式设计，工位通过 ChannelId 推导。
// - MyStationIndex = Context.ChannelId - FirstChannelId + 1
// - 默认不再弹窗提示，整个流程适用于机台自动动作。
// - 触摸步骤/PLC 动作使用参数化 DSL 配置，避免把机台点位写死在脚本里。
// ============================================

CancellationToken GetExecutionToken() => Context?.CancellationToken ?? CancellationToken.None;
void ThrowIfStopRequested() => GetExecutionToken().ThrowIfCancellationRequested();


BYDMes _mes = new BYDMes();
List<BYDMES.SfcKeyInfo> sfcKeyList = new List<BYDMES.SfcKeyInfo>();
string _mesFinalSummary = "";
string _pcbaSn = "";
string _deviceMac = "";

ProcessAsync _processAsync = new ProcessAsync();
ProcessAsyncFunctions.IProcessHandle _shellHandle = null;
bool _shellConnected = false;
bool _enteredTestMode = false;

InovancePLCModBus.InovancePLC PLC = new InovancePLCModBus.InovancePLC();
bool _plcConnected = false;
bool _plcAbortResetSent = false;

List<KeyTouchStep> _steps = new List<KeyTouchStep>();
Dictionary<string, int> _stepCapSignals = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
int _eventPassedCount = 0;
int _capPassedCount = 0;
bool _testPassed = false;


struct KeyTouchStep
{
    public string Name { get; set; }
    public string ExpectedEvent { get; set; }
    public int? CapId { get; set; }
    public string ActionCommands { get; set; }
    public string ReleaseCommands { get; set; }

    public override string ToString()
    {
        return $"{Name ?? ""} => event[{ExpectedEvent ?? ""}] cap[{(CapId.HasValue ? CapId.Value.ToString() : "-")}]";
    }
}

string ProductModel => Param<string>("ProductModel", "V3");
bool MESEnble => Param<bool>("MESEnble", false);
string MacSourceMode => Param<string>("MacSourceMode", "MES_PCBA_SN");

// ===== 产线常用参数 =====
// BLE_Port_Station1 / BLE_Port_Station2: 双工位硬件串口
// KeyTest_StepPlan_Station1 / KeyTest_StepPlan_Station2: 现场覆盖默认步骤
// KeyCap_MinSignal / KeyCap_MaxSignal: 电容判定范围，默认 5000~80000
// PLC_LogPollingReads: true 时打印每次 PLC 轮询读值，默认 false 减少刷屏
// FocusNextChannelAfterScan: 扫码触发测试后立即把光标切到下一工位
// ClearSerialNumberOnFinish / FocusCurrentChannelOnFinish: 测试结束后清空/聚焦当前工位扫码框
// BLE_ShellQuitWaitMs: 发送 quit 后等待 v3.exe 自行释放 BLE/串口的时间
int TotalStations => Param<int>("TotalStations", 2);
int FirstChannelId => Param<int>("FirstChannelId", 1);
int MyStationIndex => Math.Max(1, Context.ChannelId - FirstChannelId + 1);
int NextStationIndex => MyStationIndex >= TotalStations ? 1 : MyStationIndex + 1;
int NextChannelId => FirstChannelId + NextStationIndex - 1;
string SerialNumberInputRole => Param<string>("SerialNumberInputRole", "txtSerialNumber");
string SerialNumberInputDataKey => Param<string>("SerialNumberInputDataKey", "");
bool FocusNextChannelAfterScan => Param<bool>("FocusNextChannelAfterScan", true);
bool ClearSerialNumberOnFinish => Param<bool>("ClearSerialNumberOnFinish", true);
bool FocusCurrentChannelOnFinish => Param<bool>("FocusCurrentChannelOnFinish", true);

string V3ExePath => Param<string>("V3_ExePath", @"C:\bat\v3_exe.bat");
int BleConnectMode => Param<int>("BLE_ConnectMode", 3); // 1=按MacSourceMode解析MAC, 2=参数MAC, 3=自动最强信号
string BleDeviceMac => Param<string>("BLE_DeviceMac", "");
string BlePort => ResolveBlePortParam();
int BleConnectTimeoutMs => Param<int>("BLE_ConnectTimeoutMs", 15000);
int BleCommandTimeoutMs => Param<int>("BLE_CommandTimeoutMs", 10000);
int BleShellQuitWaitMs => Param<int>("BLE_ShellQuitWaitMs", 2000);
bool BleEnterTestMode => Param<bool>("BLE_EnterTestMode", true);
int MacStartIndex => Param<int>("MacStartIndex", 4);
int MacLength => Param<int>("MacLength", 12);

string PlcSeries => Param<string>("PLC_Series", "H5U");
string PlcIpAddress => Param<string>("PLC_IpAddress", "192.168.1.88");
int PlcPort => Param<int>("PLC_Port", 502);
int PlcCommandGapMs => Param<int>("PLC_CommandGapMs", 80);
bool PlcAutoTrigger => Param<bool>("PLC_AutoTrigger", true);
bool PlcUseStepHandshake => Param<bool>("PLC_UseStepHandshake", true);
bool PlcWaitKeyDoneAfterStepDone => Param<bool>("PLC_WaitKeyDoneAfterStepDone", true);
bool PlcReleasePositionAfterKeyDone => Param<bool>("PLC_ReleasePositionAfterKeyDone", true);
bool PlcWaitKeyDoneResetAfterStep => Param<bool>("PLC_WaitKeyDoneResetAfterStep", false);
bool PlcEnsureKeyDoneIdleBeforeStep => Param<bool>("PLC_EnsureKeyDoneIdleBeforeStep", false);
int PlcMBase => ResolveStationIntParam("PLC_MBase", MyStationIndex == 1 ? 200 : 220);
int PlcKeyDoneM => ResolveStationIntParam("PLC_KeyDoneM", PlcMBase + 10);
int PlcTestDoneM => ResolveStationIntParam("PLC_TestDoneM", MyStationIndex == 1 ? 211 : 231);
int PlcAbortResetM => ResolveStationIntParam("PLC_AbortResetM", MyStationIndex == 1 ? 240 : 241);
int PlcSwitchForwardM => ResolveStationIntParam("PLC_SwitchForwardM", PlcMBase + 12);
int PlcSwitchResetM => ResolveStationIntParam("PLC_SwitchResetM", PlcMBase + 13);
int PlcPositionReadyBase => ResolveStationIntParam("PLC_PositionReadyBase", MyStationIndex == 1 ? 250 : 270);
int PlcStepDoneBase => ResolveStationIntParam("PLC_StepDoneBase", MyStationIndex == 1 ? 260 : 280);
int PlcStepHandshakeCount => ResolveStationIntParam("PLC_StepHandshakeCount", 8);
bool PlcResetStepDoneOnStart => Param<bool>("PLC_ResetStepDoneOnStart", true);
int PlcPositionReadyTimeoutMs => ResolveStationIntParam("PLC_PositionReadyTimeoutMs", PlcKeyDoneTimeoutMs);
int PlcPositionReadyPollMs => ResolveStationIntParam("PLC_PositionReadyPollMs", PlcKeyDonePollMs);
int PlcStepDonePulseMs => ResolveStationIntParam("PLC_StepDonePulseMs", 0);
int PlcPositionSettleMs => ResolveStationIntParam("PLC_PositionSettleMs", 500);
int PlcKeyDoneTimeoutMs => ResolveStationIntParam("PLC_KeyDoneTimeoutMs", 8000);
int PlcKeyDonePollMs => ResolveStationIntParam("PLC_KeyDonePollMs", 50);
int PlcKeyDoneResetTimeoutMs => ResolveStationIntParam("PLC_KeyDoneResetTimeoutMs", 1500);
int PlcOpenTimeoutMs => ResolveStationIntParam("PLC_OpenTimeoutMs", 5000);
int PlcTcpConnectTimeoutMs => ResolveStationIntParam("PLC_TcpConnectTimeoutMs", PlcOpenTimeoutMs);
int PlcTcpSendTimeoutMs => ResolveStationIntParam("PLC_TcpSendTimeoutMs", 2000);
int PlcTcpReceiveTimeoutMs => ResolveStationIntParam("PLC_TcpReceiveTimeoutMs", 2000);
bool PlcLogPollingReads => Param<bool>("PLC_LogPollingReads", false);

int KeyWaitTimeoutMs => Param<int>("KeyTest_TimeoutMs", 5000);
string KeyWaitCommand => Param<string>("KeyTest_WaitCommand", "wait-event");
bool KeyClearEventBeforeWait => Param<bool>("KeyTest_ClearEventBeforeWait", true);
bool KeyClearEventAfterRead => Param<bool>("KeyTest_ClearEventAfterRead", true);
string KeyClearCommand => Param<string>("KeyTest_ClearCommand", "clear-event");
int KeyClearSettleMs => Param<int>("KeyTest_ClearSettleMs", 120);
int KeyActionSettleMs => Param<int>("KeyTest_ActionSettleMs", 0);
int KeyReleaseSettleMs => Param<int>("KeyTest_ReleaseSettleMs", 120);
int KeyStepRetryCount => Param<int>("KeyTest_RetryCount", 3);
int KeyRetryDelayMs => Param<int>("KeyTest_RetryDelayMs", 200);

bool EnableCapValidation => Param<bool>("KeyCap_Enable", true);
int KeyCapReadCount => Param<int>("KeyCap_ReadCount", 1);
int KeyCapReadIntervalMs => Param<int>("KeyCap_ReadIntervalMs", 80);
int KeyCapMinSignal => ResolveStationIntParam("KeyCap_MinSignal", 5000);
int KeyCapMaxSignal => ResolveStationIntParam("KeyCap_MaxSignal", 80000);

string PlcPrepareCommands => ResolveStationStringParam("PLC_PrepareCommands", "");
string PlcFinishCommands => ResolveStationStringParam("PLC_FinishCommands", "");
string KeyStepPlan => ResolveStationStringParam("KeyTest_StepPlan", "");
bool KeyUseDefaultPlanWhenEmpty => Param<bool>("KeyTest_UseDefaultPlanWhenEmpty", true);

string ResolveBlePortParam()
{
    string value = MyStationIndex switch
    {
        1 => Param<string>("BLE_Port_Station1", "COM7"),
        2 => Param<string>("BLE_Port_Station2", "COM8"),
        _ => Param<string>($"BLE_Port_Station{MyStationIndex}", null)
    };

    if (!string.IsNullOrWhiteSpace(value))
    {
        return value;
    }

    return Param<string>("BLE_Port", "");
}

string ResolveStationStringParam(string key, string fallback)
{
    string stationSpecificKey = $"{key}_Station{MyStationIndex}";
    string value = Param<string>(stationSpecificKey, null);
    if (!string.IsNullOrWhiteSpace(value))
    {
        return value;
    }

    return Param<string>(key, fallback);
}

int ResolveStationIntParam(string key, int fallback)
{
    string stationSpecificKey = $"{key}_Station{MyStationIndex}";
    string raw = Param<string>(stationSpecificKey, "");
    if (!string.IsNullOrWhiteSpace(raw) && int.TryParse(raw, out int parsed))
    {
        return parsed;
    }

    return Param<int>(key, fallback);
}

string BuildStepPlanExample()
{
    if (MyStationIndex == 2)
    {
        return BuildStation2DefaultPlan();
    }

    return string.Join(" | ", new[]
    {
        "mode=>[KEY] 按键3=>2=>M200=1=>M200=0",
        "program=>[KEY] 按键5=>4=>M201=1=>M201=0",
        "speed=>[KEY] 按键4=>3=>M202=1=>M202=0",
        "right=>[KEY] 按键7=>6=>M203=1=>M203=0",
        "start/pause=>[KEY] 按键2=>1=>M204=1=>M204=0",
        "left=>[KEY] 按键6=>5=>M205=1=>M205=0",
        "power=>[KEY] 按键1=>-=>M206=1=>M206=0",
        "switch=>[ENCODER] 编码器11@@[ENCODER] 编码器10=>-=>M212=1@@M207=1=>M207=0@@M212=0"
    });
}

string BuildStation2DefaultPlan()
{
    return string.Join(" | ", new[]
    {
        "mode=>[KEY] 按键3=>2=>M220=1=>M220=0",
        "program=>[KEY] 按键5=>4=>M221=1=>M221=0",
        "speed=>[KEY] 按键4=>3=>M222=1=>M222=0",
        "right=>[KEY] 按键7=>6=>M223=1=>M223=0",
        "start/pause=>[KEY] 按键2=>1=>M224=1=>M224=0",
        "left=>[KEY] 按键6=>5=>M225=1=>M225=0",
        "power=>[KEY] 按键1=>-=>M226=1=>M226=0",
        "switch=>[ENCODER] 编码器11@@[ENCODER] 编码器10=>-=>M232=1@@M227=1=>M227=0@@M232=0"
    });
}

[UIAction("test.start")]
[UIAction("input.serialnumber")]
async Task MainTest(string serialnumber)
{
    try
    {
        Context.SerialNumber = serialnumber?.Trim() ?? "";
        _plcAbortResetSent = false;

        PublishStatus("状态", "运行中");
        PublishProgress("测试进度", 0, "开始");
        PublishText("测试", "━━━━━ V3 双工位触摸按键自动测试 ━━━━━");
        PublishText("测试", $"工位序号 = {MyStationIndex}/{TotalStations} (ChannelId={Context.ChannelId})");
        PublishText("测试", $"产品条码 = {Context.SerialNumber}");
        PublishValue("测试条码", Context.SerialNumber, true);

        await PassFocusToNextChannelAsync();
        MesStart();
        ShowConfiguration();
        LoadAndValidateSteps();

        await ConnectBleShell();
        ThrowIfStopRequested();

        if (BleEnterTestMode)
        {
            await EnterTestMode();
            ThrowIfStopRequested();
        }

        await ExecuteTouchWorkflow();

        RecordResults();
        PublishProgress("测试进度", 100, "完成");
        PublishStatus("状态", "测试完成");
    }
    catch
    {
        await EnsurePlcAbortResetSequenceAsync("异常退出");
        throw;
    }
    finally
    {
    }
}

[UIAction("test.stop")]
void StopTest()
{
    ClosePlcClient();
    CloseShell();
    PublishStatus("状态", "已停止");
    PublishText("系统", "测试已停止");
}

[UIAction("MESTurnOn")]
public void OnSwitchToggled(bool isOn) => SetParam("MESEnble", isOn);

[UIAction("combobox.selectionchanged")]
void OnModelSelected(string model)
{
    if (!string.IsNullOrWhiteSpace(model))
    {
        SetParam("ProductModel", model);
        PublishText("配置", $"机型已切换为: {model}");
    }
}

async Task PassFocusToNextChannelAsync()
{
    if (!FocusNextChannelAfterScan || TotalStations <= 1)
    {
        return;
    }

    PublishText("流转", $">>> 光标流转: 工位{MyStationIndex} -> 工位{NextStationIndex}");
    await SendToChannelAsync(NextChannelId, "system.control.focus", new
    {
        Role = SerialNumberInputRole,
        DataKey = SerialNumberInputDataKey,
        SourceChannel = Context.ChannelId,
        Timestamp = DateTime.UtcNow
    });
}

void ResetCurrentSerialNumberInput()
{
    if (ClearSerialNumberOnFinish)
    {
        UIClearByRole(SerialNumberInputRole, SerialNumberInputDataKey);
    }

    if (FocusCurrentChannelOnFinish)
    {
        UIFocusByRole(SerialNumberInputRole, SerialNumberInputDataKey);
    }
}

void ShowConfiguration()
{
    PublishText("配置", "━━━━━ 参数配置 ━━━━━");
    PublishText("配置", $"机型 = {ProductModel}");
    PublishText("配置", $"工位 = {MyStationIndex}/{TotalStations}");
    PublishText("配置", $"PLC自动触发 = {PlcAutoTrigger}");
    PublishText("配置", $"PLC单点握手 = {PlcUseStepHandshake}");
    PublishText("配置", $"PLC地址 = {PlcIpAddress}:{PlcPort}");
    PublishText("配置", $"PLC系列 = {PlcSeries}");
    PublishText("配置", $"当前工位位置点范围= M{PlcMBase}~M{PlcMBase + 9}");
    PublishText("配置", $"当前工位位置到位返回 = M{PlcPositionReadyBase}~M{PlcPositionReadyBase + 9}");
    PublishText("配置", $"当前工位单点测试完成输出 = M{PlcStepDoneBase} (所有步骤共用)");
    PublishText("配置", $"当前工位按键完成返回 = M{PlcKeyDoneM}");
    PublishText("配置", $"当前工位正常/失败完成输出 = M{PlcTestDoneM}");
    PublishText("配置", $"当前工位异常大复位输出 = M{PlcAbortResetM}");
    PublishText("配置", $"单点完成后等待共用完成返回 = {PlcWaitKeyDoneAfterStepDone}");
    PublishText("配置", $"共用完成后释放位置点 = {PlcReleasePositionAfterKeyDone}");
    PublishText("配置", $"共用完成后等待复位 = {PlcWaitKeyDoneResetAfterStep}");
    PublishText("配置", $"当前工位switch气缸前推 = M{PlcSwitchForwardM}");
    PublishText("配置", $"当前工位switch气缸复位 = M{PlcSwitchResetM}");
    PublishText("配置", $"PLC位置到位等待 = {PlcPositionSettleMs} ms");
    PublishText("配置", $"PLC位置到位超时 = {PlcPositionReadyTimeoutMs} ms");
    PublishText("配置", $"PLC单点完成脉冲 = {PlcStepDonePulseMs} ms");
    PublishText("配置", $"PLC按键完成超时 = {PlcKeyDoneTimeoutMs} ms");
    PublishText("配置", $"PLC连接超时 = {PlcOpenTimeoutMs} ms");
    PublishText("配置", $"PLC TCP连接超时 = {PlcTcpConnectTimeoutMs} ms");
    PublishText("配置", $"PLC TCP发送超时 = {PlcTcpSendTimeoutMs} ms");
    PublishText("配置", $"PLC TCP接收超时 = {PlcTcpReceiveTimeoutMs} ms");
    PublishText("配置", $"PLC轮询读值日志 = {PlcLogPollingReads}");
    PublishText("配置", $"BLE Shell = {V3ExePath}");
    PublishText("配置", $"BLE连接模式 = {BleConnectMode} ({GetBleConnectModeName()})");
    PublishText("配置", $"BLE串口 = {(string.IsNullOrWhiteSpace(BlePort) ? "自动检测" : BlePort)}");
    PublishText("配置", $"参数MAC = {BleDeviceMac}");
    PublishText("配置", $"目标MAC = {_deviceMac}");
    PublishText("配置", $"等待事件命令 = {KeyWaitCommand} {KeyWaitTimeoutMs}");
    PublishText("配置", $"步骤重试次数 = {KeyStepRetryCount}");
    PublishText("配置", $"重试间隔 = {KeyRetryDelayMs} ms");
    PublishText("配置", $"电容范围= {KeyCapMinSignal}~{KeyCapMaxSignal}");
    PublishText("配置", $"单次电容读取次数 = {KeyCapReadCount}");
    PublishText("配置", $"MAC来源 = {MacSourceMode}");

    if (string.IsNullOrWhiteSpace(KeyStepPlan))
    {
        PublishText("配置", "KeyTest_StepPlan 未配置");
        PublishText("配置", $"建议参数= KeyTest_StepPlan_Station{MyStationIndex}");
        PublishText("配置", $"默认步骤计划 = {(KeyUseDefaultPlanWhenEmpty ? "启用" : "禁用")}");
        PublishText("配置", $"示例/默认: {BuildStepPlanExample()}");
    }
}

void LoadAndValidateSteps()
{
    _steps.Clear();
    _stepCapSignals.Clear();
    _eventPassedCount = 0;
    _capPassedCount = 0;
    bool switchStepAdded = false;

    string plan = KeyStepPlan;
    if (string.IsNullOrWhiteSpace(plan))
    {
        if (!KeyUseDefaultPlanWhenEmpty)
        {
            throw new InvalidOperationException("KeyTest_StepPlan 未配置，且 KeyTest_UseDefaultPlanWhenEmpty=false");
        }

        plan = BuildStepPlanExample();
        PublishText("配置", $"KeyTest_StepPlan 未配置，使用默认步骤计划: {plan}");
    }

    foreach (string rawStep in plan.Split(new[] { '|', '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries))
    {
        string item = rawStep.Trim();
        if (string.IsNullOrWhiteSpace(item))
        {
            continue;
        }

        string[] parts = item.Split(new[] { "=>" }, StringSplitOptions.None);
        if (parts.Length < 4)
        {
            throw new InvalidOperationException($"步骤配置格式错误: {item}");
        }

        string name = parts[0].Trim();
        string expectedEvent = parts[1].Trim();
        string capRaw = parts[2].Trim();
        string actionCommands = parts[3].Trim();
        string releaseCommands = parts.Length >= 5 ? parts[4].Trim() : "";
        bool isSwitchStep = IsSwitchStep(name);

        if (isSwitchStep && switchStepAdded)
        {
            PublishText("配置", $"已忽略重复旋钮开关步骤: {name}");
            continue;
        }

        int? capId = null;
        if (!string.IsNullOrWhiteSpace(capRaw) &&
            capRaw != "-" &&
            capRaw != "none" &&
            capRaw != "NONE")
        {
            if (!int.TryParse(capRaw, out int parsedCap))
            {
                throw new InvalidOperationException($"步骤 [{name}] 的 capId 无法解析: {capRaw}");
            }
            capId = parsedCap;
        }

        if (IsNoCapStep(name))
        {
            if (capId.HasValue)
            {
                PublishText("配置", $"步骤[{name}] 为非电容步骤，已忽略 capId={capId.Value}");
            }

            capId = null;
        }

        if (isSwitchStep)
        {
            expectedEvent = "[ENCODER]";
            switchStepAdded = true;
        }

        if (PlcAutoTrigger && string.IsNullOrWhiteSpace(actionCommands))
        {
            throw new InvalidOperationException($"步骤 [{name}] 缺少 PLC 动作命令");
        }

        _steps.Add(new KeyTouchStep
        {
            Name = name,
            ExpectedEvent = expectedEvent,
            CapId = capId,
            ActionCommands = actionCommands,
            ReleaseCommands = releaseCommands
        });
    }

    if (_steps.Count == 0)
    {
        throw new InvalidOperationException("KeyTest_StepPlan 解析后为空");
    }

    PublishText("配置", $"步骤数= {_steps.Count}");
    PublishText("配置", "━━━━━ 最终展开步骤 ━━━━━");
    for (int i = 0; i < _steps.Count; i++)
    {
        PublishText("配置", $"步骤{i + 1}: {DescribeStepForLine(_steps[i])}");
    }
}

string DescribeStepForLine(KeyTouchStep step)
{
    string capText = step.CapId.HasValue ? $"cap{step.CapId.Value}" : "无电容";
    return $"{step.Name} | {step.ExpectedEvent} | {capText} | 动作[{step.ActionCommands}] | 释放[{step.ReleaseCommands}]";
}

void ResolveBleMac()
{
    string mode = (MacSourceMode ?? "MES_PCBA_SN").Trim().ToUpperInvariant();

    if (mode == "DIRECT_MAC")
    {
        if (!TryNormalizeMac(Context.SerialNumber, out _deviceMac))
        {
            throw new InvalidOperationException($"无法从条码直接识别 MAC: {Context.SerialNumber}");
        }

        PublishText("MAC解析", $"直接使用条码作为 MAC: {_deviceMac}");
        return;
    }

    if (mode == "DIRECT_SN")
    {
        if (!TryParseMacFromText(Context.SerialNumber, out _deviceMac))
        {
            throw new InvalidOperationException($"无法从条码中按位置解析 MAC: {Context.SerialNumber}");
        }

        PublishText("MAC解析", $"直接从条码解析 MAC: {_deviceMac}");
        return;
    }

    MESgetsfckeybysfc();
    if (!TryParseMacFromText(_pcbaSn, out _deviceMac))
    {
        throw new InvalidOperationException($"无法从 MES 主板条码解析 MAC: {_pcbaSn}");
    }

    PublishText("MAC解析", $"MES 主板条码 -> MAC: {_deviceMac}");
}

bool TryNormalizeMac(string raw, out string macAddress)
{
    macAddress = "";
    if (string.IsNullOrWhiteSpace(raw))
    {
        return false;
    }

    string hex = raw.Replace(":", "").Replace("-", "").Trim().ToUpperInvariant();
    if (hex.Length != 12 || hex.Any(c => !((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))))
    {
        return false;
    }

    macAddress = string.Join(":", Enumerable.Range(0, 6).Select(i => hex.Substring(i * 2, 2)));
    return true;
}

bool TryParseMacFromText(string text, out string macAddress)
{
    macAddress = "";
    if (string.IsNullOrWhiteSpace(text))
    {
        return false;
    }

    if (text.Length < MacStartIndex + MacLength)
    {
        PublishText("MAC解析", $"长度不足: {text.Length} < {MacStartIndex + MacLength}");
        return false;
    }

    string hex = text.Substring(MacStartIndex, MacLength).Trim().ToUpperInvariant();
    if (!TryNormalizeMac(hex, out macAddress))
    {
        PublishText("MAC解析", $"MAC 片段无效: {hex}");
        return false;
    }

    PublishText("MAC解析", $"提取位置: [{MacStartIndex}:{MacStartIndex + MacLength}] = {hex}");
    return true;
}

string GetBleConnectModeName()
{
    return BleConnectMode switch
    {
        1 => "按MacSourceMode解析MAC",
        2 => "参数指定MAC",
        3 => "自动最强信号",
        _ => "未知"
    };
}

bool IsNoCapStep(string stepName)
{
    string normalized = (stepName ?? "").Trim().ToLowerInvariant();
    return normalized == "power" || IsSwitchStep(stepName);
}

bool IsSwitchStep(string stepName)
{
    string normalized = (stepName ?? "").Trim().ToLowerInvariant();
    return normalized == "switch" || normalized.StartsWith("switch-");
}

int ResolvePlcPointIndex(KeyTouchStep step, int stepIndex)
{
    if (IsSwitchStep(step.Name))
    {
        return 7;
    }

    return stepIndex;
}

string BuildBleConnectArguments()
{
    _deviceMac = "";
    string portArguments = BuildBlePortArguments();

    if (BleConnectMode == 1)
    {
        ResolveBleMac();
        return JoinArguments(portArguments, $"--mac {_deviceMac}");
    }

    if (BleConnectMode == 2)
    {
        if (!TryNormalizeMac(BleDeviceMac, out _deviceMac))
        {
            throw new InvalidOperationException("连接模式2要求参数 BLE_DeviceMac 为有效MAC");
        }

        PublishText("MAC解析", $"使用参数 MAC: {_deviceMac}");
        return JoinArguments(portArguments, $"--mac {_deviceMac}");
    }

    if (BleConnectMode == 3)
    {
        PublishText("BLE", "连接模式3：不指定MAC，自动扫描并连接最强信号设备");
        return portArguments;
    }

    throw new InvalidOperationException($"不支持的 BLE_ConnectMode: {BleConnectMode}");
}

string BuildBlePortArguments()
{
    string port = (BlePort ?? "").Trim();
    if (string.IsNullOrWhiteSpace(port))
    {
        return "";
    }

    PublishText("BLE", $"指定硬件串口: {port}");
    return $"--port {QuoteArgument(port)}";
}

string JoinArguments(params string[] parts)
{
    return string.Join(" ", parts.Where(part => !string.IsNullOrWhiteSpace(part)));
}

string QuoteArgument(string value)
{
    if (string.IsNullOrWhiteSpace(value))
    {
        return "\"\"";
    }

    if (value.IndexOfAny(new[] { ' ', '\t', '"' }) < 0)
    {
        return value;
    }

    return "\"" + value.Replace("\"", "\\\"") + "\"";
}

async Task ConnectBleShell()
{
    string arguments = BuildBleConnectArguments();

    PublishText("BLE", "━━━━━ BLE 连接 ━━━━━");
    PublishText("BLE", $"连接模式 = {BleConnectMode} ({GetBleConnectModeName()})");
    PublishText("BLE", $"目标设备 = {(string.IsNullOrWhiteSpace(_deviceMac) ? "自动最强信号" : _deviceMac)}");
    PublishText("BLE", $"启动参数 = {(string.IsNullOrWhiteSpace(arguments) ? "<empty>" : arguments)}");
    PublishProgress("测试进度", 10, "BLE连接");

    _processAsync.SetArgumentsAndWorkingDirectory(arguments, null);
    _shellHandle = _processAsync.Create(V3ExePath, ProcessWindowStyleOption.Hidden);

    string output = await ProcessAsync.SendCommandAndReadRegexAsync(
        _shellHandle,
        "",
        @"(V3>|Connected|ERROR|FAIL|失败)",
        BleConnectTimeoutMs,
        GetExecutionToken());

    PublishText("BLE输出", output);

    if (output.Contains("V3>") || output.Contains("Connected") || output.Contains("连接成功"))
    {
        _shellConnected = true;
        PublishValue("BLE_CONNECT", "", true);
        await SleepAsync(200);
        return;
    }

    PublishValue("BLE_CONNECT", "", false);
    throw new Exception("BLE 连接失败");
}

async Task EnterTestMode()
{
    PublishText("BLE", "进入产测模式...");
    string output = await SendShellCommand("enter", BleCommandTimeoutMs);
    PublishText("BLE输出", output);

    if (output.Contains("[OK]") || output.Contains("test mode") || output.Contains("产测模式"))
    {
        _enteredTestMode = true;
        PublishValue("ENTER_TEST_MODE", "", true);
        await SleepAsync(200);
        return;
    }

    PublishValue("ENTER_TEST_MODE", "", false);
    throw new Exception("进入产测模式失败");
}

async Task ExecuteTouchWorkflow()
{
    if (PlcAutoTrigger)
    {
        await OpenPlcClientAsync();
        try
        {
            EnsurePlcTestDoneIdle();
            await ResetPlcStepDoneOutputsAsync();

            if (!string.IsNullOrWhiteSpace(PlcPrepareCommands))
            {
                PublishText("PLC", $"执行测试前动作: {PlcPrepareCommands}");
                await ExecutePlcCommandList(PlcPrepareCommands);
            }

            bool sequencePassed = await ExecuteStepSequence();
            await SendPlcTestDoneAsync(sequencePassed ? "测试正常完成" : "测试失败完成");

            if (!string.IsNullOrWhiteSpace(PlcFinishCommands))
            {
                PublishText("PLC", $"执行测试结束动作: {PlcFinishCommands}");
                await ExecutePlcCommandList(PlcFinishCommands);
            }
        }
        catch
        {
            await EnsurePlcAbortResetSequenceAsync("异常退出");
            throw;
        }
        finally
        {
            ClosePlcClient();
        }
    }
    else
    {
        await ExecuteStepSequence();
    }
}

async Task<bool> ExecuteStepSequence()
{
    PublishText("按键", "━━━━━ 按键/电容自动测试 ━━━━━");

    for (int i = 0; i < _steps.Count; i++)
    {
        ThrowIfStopRequested();

        KeyTouchStep step = _steps[i];
        PublishProgress("测试进度", 20 + (int)((i / (double)_steps.Count) * 60), $"步骤{i + 1}");
        PublishText("按键", $"步骤 {i + 1}/{_steps.Count}: {step.Name}");
        PublishText("按键", $"期望事件: {step.ExpectedEvent}");

        bool eventPassed = false;
        string eventFailure = "未知失败";
        bool needCap = EnableCapValidation && step.CapId.HasValue;
        bool capPassed = !needCap;
        string capFailure = "未读取";
        int capAttemptsUsed = 0;
        int maxAttempts = Math.Max(1, KeyStepRetryCount);
        string lastEventOutput = "";

        for (int attempt = 1; attempt <= maxAttempts; attempt++)
        {
            PublishText("按键", $"步骤[{step.Name}] 事件尝试 {attempt}/{maxAttempts}");

            if (KeyClearEventBeforeWait)
            {
                string clearOutput = await SendShellCommand(KeyClearCommand, BleCommandTimeoutMs);
                PublishText("按键清理输出", clearOutput);
                if (KeyClearSettleMs > 0)
                {
                    await SleepAsync(KeyClearSettleMs);
                }
            }

            string eventOutput = "";
            int capSignal = int.MinValue;
            bool capReadThisAttempt = false;

            await RunStepActionAsync(step, i, async () =>
            {
                eventOutput = await SendShellCommand(
                    $"{KeyWaitCommand} {KeyWaitTimeoutMs}",
                    Math.Max(BleCommandTimeoutMs, KeyWaitTimeoutMs + 3000));

                if (needCap && IsExpectedEvent(eventOutput, step.ExpectedEvent))
                {
                    capAttemptsUsed++;
                    PublishText("电容", $"步骤[{step.Name}] 电容尝试 {capAttemptsUsed}/{maxAttempts}（同次按压）");
                    capSignal = await ReadCapSignal(step.CapId.Value);
                    capReadThisAttempt = true;
                }

                if (KeyClearEventAfterRead)
                {
                    await ClearInputEventAsync("按键读后清理");
                }
            });
            lastEventOutput = eventOutput;

            PublishText("按键输出", eventOutput);

            if (IsExpectedEvent(eventOutput, step.ExpectedEvent))
            {
                eventPassed = true;
                _eventPassedCount++;
                PublishValue($"按键[{step.Name}]事件", "PASS", true);

                if (!needCap)
                {
                    break;
                }

                if (capReadThisAttempt)
                {
                    _stepCapSignals[step.Name] = capSignal;

                    if (IsCapSignalInRange(capSignal))
                    {
                        PublishValue($"按键[{step.Name}]电容", capSignal, "", KeyCapMinSignal, KeyCapMaxSignal);
                        capPassed = true;
                        _capPassedCount++;
                    }
                    else
                    {
                        capFailure = BuildCapFailureMessage(step.CapId.Value, capSignal);
                        if (capAttemptsUsed >= maxAttempts)
                        {
                            PublishValue($"按键[{step.Name}]电容", capSignal, "", KeyCapMinSignal, KeyCapMaxSignal);
                            PublishText("电容", $"步骤[{step.Name}] 第{capAttemptsUsed}次失败: {capFailure}");
                        }
                    }
                }

                break;
            }

            string actualEvent = SummarizeActualEvent(eventOutput);
            eventFailure = $"事件不匹配，期望 [{step.ExpectedEvent}]，实际[{actualEvent}]";
            if (attempt >= maxAttempts)
            {
                PublishText("按键", $"步骤[{step.Name}] 第{attempt}次失败: {eventFailure}");
                PublishValue($"按键[{step.Name}]事件", actualEvent, false);
            }

            if (attempt < maxAttempts && KeyRetryDelayMs > 0)
            {
                await SleepAsync(KeyRetryDelayMs);
            }
        }

        if (!eventPassed)
        {
            FailTest($"步骤[{step.Name}]事件", eventFailure);
            PublishAlarm($"步骤[{step.Name}]事件失败", "error");
            PublishValue($"按键[{step.Name}]事件", SummarizeActualEvent(lastEventOutput), false);
            PublishText("按键", $"步骤[{step.Name}] 事件最终失败: {eventFailure}");
            return false;
        }

        if (needCap && !capPassed)
        {
            for (int attempt = capAttemptsUsed + 1; attempt <= maxAttempts; attempt++)
            {
                PublishText("电容", $"步骤[{step.Name}] 电容尝试 {attempt}/{maxAttempts}");

                int capSignal = int.MinValue;
                await RunStepActionAsync(step, i, async () =>
                {
                    capSignal = await ReadCapSignal(step.CapId.Value);
                });

                _stepCapSignals[step.Name] = capSignal;

                if (IsCapSignalInRange(capSignal))
                {
                    PublishValue($"按键[{step.Name}]电容", capSignal, "", KeyCapMinSignal, KeyCapMaxSignal);
                    capPassed = true;
                    _capPassedCount++;
                    break;
                }

                capFailure = BuildCapFailureMessage(step.CapId.Value, capSignal);
                if (attempt >= maxAttempts)
                {
                    PublishValue($"按键[{step.Name}]电容", capSignal, "", KeyCapMinSignal, KeyCapMaxSignal);
                    PublishText("电容", $"步骤[{step.Name}] 第{attempt}次失败: {capFailure}");
                }

                if (attempt < maxAttempts && KeyRetryDelayMs > 0)
                {
                    await SleepAsync(KeyRetryDelayMs);
                }
            }

            if (!capPassed)
            {
                FailTest($"步骤[{step.Name}]电容", capFailure);
                PublishAlarm($"步骤[{step.Name}]电容失败", "error");
                PublishText("电容", $"步骤[{step.Name}] 电容最终失败: {capFailure}");
                return false;
            }
        }
    }

    PassTest("触摸按键测试");
    return true;
}

bool IsCapSignalInRange(int signal)
{
    return signal >= KeyCapMinSignal && signal <= KeyCapMaxSignal;
}

string BuildCapFailureMessage(int capId, int signal)
{
    if (signal < KeyCapMinSignal)
    {
        return $"电容值过低，cap={capId}，signal={signal}，范围{KeyCapMinSignal}~{KeyCapMaxSignal}";
    }

    if (signal > KeyCapMaxSignal)
    {
        return $"电容值过高，cap={capId}，signal={signal}，范围{KeyCapMinSignal}~{KeyCapMaxSignal}";
    }

    return $"电容值异常，cap={capId}，signal={signal}，范围{KeyCapMinSignal}~{KeyCapMaxSignal}";
}

async Task RunStepActionAsync(KeyTouchStep step, int stepIndex, Func<Task> body)
{
    int plcPointIndex = ResolvePlcPointIndex(step, stepIndex);
    bool actionExecuted = false;
    bool keyDoneSeen = false;
    bool bodyCompleted = false;

    try
    {
        if (PlcAutoTrigger && !string.IsNullOrWhiteSpace(step.ActionCommands))
        {
            if (PlcEnsureKeyDoneIdleBeforeStep)
            {
                await EnsurePlcKeyDoneIdleAsync(step.Name);
            }

            await EnsurePlcStepDoneIdleAsync(step.Name, plcPointIndex);

            PublishText("PLC", $"动作命令: {step.ActionCommands}");
            await ExecutePlcCommandList(step.ActionCommands);
            actionExecuted = true;

            if (PlcUseStepHandshake)
            {
                await WaitPlcPositionReadyAsync(step.Name, plcPointIndex);
                await ExecutePlcAfterReadyCommandList(step.ActionCommands);
            }

            if (PlcPositionSettleMs > 0)
            {
                PublishText("PLC", $"等待位置动作稳定 {PlcPositionSettleMs} ms");
                await SleepAsync(PlcPositionSettleMs);
            }
        }

        if (KeyActionSettleMs > 0)
        {
            await SleepAsync(KeyActionSettleMs);
        }

        await body();
        bodyCompleted = true;

        if (PlcAutoTrigger && actionExecuted)
        {
            if (PlcUseStepHandshake)
            {
                await SendPlcStepDoneAsync(step.Name, plcPointIndex);
            }

            if (PlcWaitKeyDoneAfterStepDone)
            {
                await WaitPlcKeyDoneAsync(step.Name);
                keyDoneSeen = true;
            }
        }
    }
    finally
    {
        if (PlcAutoTrigger && actionExecuted && !bodyCompleted && PlcUseStepHandshake)
        {
            try
            {
                await SendPlcStepDoneAsync(step.Name, plcPointIndex);
            }
            catch (Exception doneEx)
            {
                PublishText("PLC", $"单点完成输出异常: {doneEx.Message}");
            }
        }

        if (PlcAutoTrigger &&
            actionExecuted &&
            PlcReleasePositionAfterKeyDone &&
            !string.IsNullOrWhiteSpace(step.ReleaseCommands))
        {
            try
            {
                PublishText("PLC", $"释放命令: {step.ReleaseCommands}");
                await ExecutePlcCommandList(step.ReleaseCommands);
            }
            catch (Exception releaseEx)
            {
                PublishText("PLC", $"释放命令异常: {releaseEx.Message}");
            }
        }

        if (KeyReleaseSettleMs > 0)
        {
            await SleepAsync(KeyReleaseSettleMs);
        }

        if (PlcAutoTrigger && keyDoneSeen && PlcWaitKeyDoneResetAfterStep && PlcKeyDoneResetTimeoutMs > 0)
        {
            await WaitPlcKeyDoneResetAsync(step.Name);
        }
    }
}

bool IsExpectedEvent(string output, string expectedEvent)
{
    if (string.IsNullOrWhiteSpace(output))
    {
        return false;
    }

    if (string.IsNullOrWhiteSpace(expectedEvent))
    {
        return output.Contains("[KEY]") || output.Contains("[ENCODER]");
    }

    var expectedItems = expectedEvent
        .Split(new[] { "@@" }, StringSplitOptions.RemoveEmptyEntries)
        .Select(item => item.Trim())
        .Where(item => !string.IsNullOrWhiteSpace(item))
        .ToList();

    foreach (string item in expectedItems)
    {
        if (item.Contains("[ENCODER]"))
        {
            int? expectedEncoderNo = TryExtractEncoderNumber(item);
            if (expectedEncoderNo.HasValue)
            {
                if (ContainsEncoderEvent(output, expectedEncoderNo.Value))
                {
                    return true;
                }

                continue;
            }

            if (output.Contains("[ENCODER]"))
            {
                return true;
            }

            continue;
        }

        int? expectedKeyNo = TryExtractKeyNumber(item);
        if (expectedKeyNo.HasValue)
        {
            if (ContainsKeyDownEvent(output, expectedKeyNo.Value))
            {
                return true;
            }

            continue;
        }

        if (output.Contains(item))
        {
            return true;
        }
    }

    return false;
}

int? TryExtractKeyNumber(string text)
{
    if (string.IsNullOrWhiteSpace(text))
    {
        return null;
    }

    var match = System.Text.RegularExpressions.Regex.Match(text, @"按键\s*(\d+)", System.Text.RegularExpressions.RegexOptions.IgnoreCase);
    if (!match.Success)
    {
        match = System.Text.RegularExpressions.Regex.Match(text, @"\bkey\s*[:=]?\s*(\d+)\b", System.Text.RegularExpressions.RegexOptions.IgnoreCase);
    }

    if (match.Success && int.TryParse(match.Groups[1].Value, out int keyNo))
    {
        return keyNo;
    }

    return null;
}

int? TryExtractEncoderNumber(string text)
{
    if (string.IsNullOrWhiteSpace(text))
    {
        return null;
    }

    var match = System.Text.RegularExpressions.Regex.Match(text, @"编码器\s*(\d+)", System.Text.RegularExpressions.RegexOptions.IgnoreCase);
    if (!match.Success)
    {
        match = System.Text.RegularExpressions.Regex.Match(text, @"\bencoder\s*[:=]?\s*(\d+)\b", System.Text.RegularExpressions.RegexOptions.IgnoreCase);
    }

    if (match.Success && int.TryParse(match.Groups[1].Value, out int encoderNo))
    {
        return encoderNo;
    }

    return null;
}

bool ContainsKeyDownEvent(string output, int expectedKeyNo)
{
    var keyLines = output
        .Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries)
        .Select(line => line.Trim())
        .Where(line => line.Contains("[KEY]"));

    foreach (string line in keyLines)
    {
        if (IsKeyReleaseLine(line))
        {
            continue;
        }

        int? actualKeyNo = TryExtractKeyNumber(line);
        if (actualKeyNo.HasValue && actualKeyNo.Value == expectedKeyNo)
        {
            return true;
        }
    }

    return false;
}

bool ContainsEncoderEvent(string output, int expectedEncoderNo)
{
    var encoderLines = output
        .Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries)
        .Select(line => line.Trim())
        .Where(line => line.Contains("[ENCODER]"));

    foreach (string line in encoderLines)
    {
        int? actualEncoderNo = TryExtractEncoderNumber(line);
        if (actualEncoderNo.HasValue && actualEncoderNo.Value == expectedEncoderNo)
        {
            return true;
        }
    }

    return false;
}

bool IsKeyReleaseLine(string line)
{
    if (string.IsNullOrWhiteSpace(line))
    {
        return false;
    }

    return line.Contains("松开") ||
        line.Contains("释放") ||
        line.Contains("抬起") ||
        line.IndexOf("release", StringComparison.OrdinalIgnoreCase) >= 0 ||
        line.IndexOf("key up", StringComparison.OrdinalIgnoreCase) >= 0 ||
        line.IndexOf("keyup", StringComparison.OrdinalIgnoreCase) >= 0 ||
        line.IndexOf(" up ", StringComparison.OrdinalIgnoreCase) >= 0;
}

async Task ClearInputEventAsync(string title)
{
    string clearOutput = await SendShellCommand(KeyClearCommand, BleCommandTimeoutMs);
    PublishText(title, clearOutput);
    if (KeyClearSettleMs > 0)
    {
        await SleepAsync(KeyClearSettleMs);
    }
}

string SummarizeActualEvent(string output)
{
    if (string.IsNullOrWhiteSpace(output))
    {
        return "无返回";
    }

    var lines = output
        .Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries)
        .Select(line => line.Trim())
        .Where(line => !string.IsNullOrWhiteSpace(line))
        .ToList();

    string actualLine = lines.LastOrDefault(line => line.Contains("[KEY]") || line.Contains("[ENCODER]"));
    return string.IsNullOrWhiteSpace(actualLine) ? output.Trim() : actualLine;
}

async Task<int> ReadCapSignal(int capId)
{
    int bestSignal = int.MinValue;

    for (int i = 0; i < Math.Max(1, KeyCapReadCount); i++)
    {
        string output = await SendShellCommand($"key-cap {capId}", BleCommandTimeoutMs);
        PublishText("电容输出", output);

        int signal = ParseCapSignal(output);
        bestSignal = Math.Max(bestSignal, signal);

        if (i + 1 < KeyCapReadCount && KeyCapReadIntervalMs > 0)
        {
            await SleepAsync(KeyCapReadIntervalMs);
        }
    }

    PublishText("电容", $"cap {capId} 最终值 = {bestSignal}");
    return bestSignal;
}

int ParseCapSignal(string output)
{
    if (string.IsNullOrWhiteSpace(output))
    {
        throw new Exception("key-cap 无返回");
    }

    const string marker = "Signal:";
    int markerIndex = output.IndexOf(marker, StringComparison.OrdinalIgnoreCase);
    if (markerIndex < 0)
    {
        throw new Exception($"无法解析 key-cap 输出: {output}");
    }

    int start = markerIndex + marker.Length;
    while (start < output.Length && char.IsWhiteSpace(output[start]))
    {
        start++;
    }

    int end = start;
    while (end < output.Length && char.IsDigit(output[end]))
    {
        end++;
    }

    if (end == start || !int.TryParse(output.Substring(start, end - start), out int signal))
    {
        throw new Exception($"无法解析 key-cap 输出: {output}");
    }

    return signal;
}

async Task OpenPlcClientAsync()
{
    PublishText("PLC", $"开始初始化 PLC 客户端: {PlcSeries}");
    PLC.SetPLCType(PlcSeries);
    PLC.DebugMode = true;
    TrySetPlcIntProperty("ConnectTimeout", PlcTcpConnectTimeoutMs);
    TrySetPlcIntProperty("SendTimeout", PlcTcpSendTimeoutMs);
    TrySetPlcIntProperty("ReceiveTimeout", PlcTcpReceiveTimeoutMs);

    PublishText("PLC", $"正在连接 PLC {PlcIpAddress}:{PlcPort} ({PlcSeries}), 超时 {PlcOpenTimeoutMs} ms");
    Task<bool> openTask = Task.Run(() => PLC.Open(PlcIpAddress, PlcPort));
    Task completedTask = await Task.WhenAny(openTask, Task.Delay(Math.Max(1, PlcOpenTimeoutMs)));
    if (completedTask != openTask)
    {
        throw new TimeoutException($"PLC 连接超时: {PlcIpAddress}:{PlcPort}, {PlcOpenTimeoutMs} ms");
    }

    bool opened = await openTask;
    if (!opened)
    {
        throw new Exception($"PLC 打开失败: {PlcIpAddress}:{PlcPort}");
    }

    _plcConnected = true;
    PublishText("PLC", $"PLC 已连接 {PlcIpAddress}:{PlcPort} ({PlcSeries})");
}

void TrySetPlcIntProperty(string propertyName, int value)
{
    try
    {
        var property = PLC.GetType().GetProperty(propertyName);
        if (property == null || !property.CanWrite)
        {
            PublishText("PLC", $"当前 PLC DLL 不支持属性 {propertyName}，已跳过");
            return;
        }

        property.SetValue(PLC, Math.Max(1, value), null);
        PublishText("PLC", $"{propertyName} = {Math.Max(1, value)} ms");
    }
    catch (Exception ex)
    {
        PublishText("PLC", $"设置 {propertyName} 失败: {ex.Message}");
    }
}

void ClosePlcClient()
{
    if (!_plcConnected)
    {
        return;
    }

    try
    {
        PLC.Close();
        PublishText("PLC", "PLC 已关闭");
    }
    catch (Exception ex)
    {
        PublishText("PLC", $"关闭 PLC 异常: {ex.Message}");
    }
    finally
    {
        _plcConnected = false;
    }
}

async Task ExecutePlcCommandList(string commands)
{
    foreach (string rawToken in commands.Split(new[] { "@@" }, StringSplitOptions.RemoveEmptyEntries))
    {
        string token = rawToken.Trim();
        if (string.IsNullOrWhiteSpace(token))
        {
            continue;
        }

        if (token.StartsWith("DELAY=", StringComparison.OrdinalIgnoreCase))
        {
            if (!int.TryParse(token.Substring("DELAY=".Length), out int delayMs))
            {
                throw new InvalidOperationException($"无法解析延时命令: {token}");
            }

            await SleepAsync(delayMs);
            continue;
        }

        if (token.StartsWith("COMMENT:", StringComparison.OrdinalIgnoreCase))
        {
            continue;
        }

        if (token.StartsWith("AFTER_READY:", StringComparison.OrdinalIgnoreCase))
        {
            continue;
        }

        ExecutePlcMCommand(token);
        if (PlcCommandGapMs > 0)
        {
            await SleepAsync(PlcCommandGapMs);
        }
    }
}

async Task ExecutePlcAfterReadyCommandList(string commands)
{
    foreach (string rawToken in commands.Split(new[] { "@@" }, StringSplitOptions.RemoveEmptyEntries))
    {
        string token = rawToken.Trim();
        if (!token.StartsWith("AFTER_READY:", StringComparison.OrdinalIgnoreCase))
        {
            continue;
        }

        string command = token.Substring("AFTER_READY:".Length).Trim();
        if (string.IsNullOrWhiteSpace(command))
        {
            continue;
        }

        PublishText("PLC", $"到位后命令: {command}");
        ExecutePlcMCommand(command);
        if (PlcCommandGapMs > 0)
        {
            await SleepAsync(PlcCommandGapMs);
        }
    }
}

async Task SendPlcTestDoneAsync(string reason = "")
{
    if (!PlcAutoTrigger)
    {
        return;
    }

    string doneAddress = $"M{PlcTestDoneM}";
    PublishText("PLC", $"{(string.IsNullOrWhiteSpace(reason) ? "发送测试完成输出" : reason)} {doneAddress}=1");
    ExecutePlcMCommand($"{doneAddress}=1");
    await SleepAsync(Math.Max(0, PlcCommandGapMs));
}

async Task SendPlcAbortResetSequenceAsync(string reason)
{
    if (!PlcAutoTrigger)
    {
        return;
    }

    bool openedHere = false;
    try
    {
        if (!_plcConnected)
        {
            await OpenPlcClientAsync();
            openedHere = true;
        }

        await TrySendPlcExitMCommandAsync(reason, $"M{PlcStepDoneBase}", "先发送单点完成复位");
        await TrySendPlcExitMCommandAsync(reason, $"M{PlcAbortResetM}", "再发送异常大复位");
    }
    catch (Exception ex)
    {
        PublishText("PLC", $"{reason}复位序列异常: {ex.Message}");
    }
    finally
    {
        if (openedHere)
        {
            ClosePlcClient();
        }
    }
}

async Task EnsurePlcAbortResetSequenceAsync(string reason)
{
    if (_plcAbortResetSent)
    {
        return;
    }

    await SendPlcAbortResetSequenceAsync(reason);
    _plcAbortResetSent = true;
}

async Task TrySendPlcExitMCommandAsync(string reason, string address, string action)
{
    try
    {
        PublishText("PLC", $"{reason}: {action} {address}=1");
        ExecutePlcMCommand($"{address}=1");
        await Task.Delay(Math.Max(0, PlcCommandGapMs));
    }
    catch (Exception ex)
    {
        PublishText("PLC", $"{reason}: {action} {address}=1 失败: {ex.Message}");
    }
}

async Task ResetPlcStepDoneOutputsAsync()
{
    if (!PlcAutoTrigger || !PlcUseStepHandshake || !PlcResetStepDoneOnStart)
    {
        return;
    }

    int count = Math.Min(1, Math.Max(0, PlcStepHandshakeCount));
    if (count <= 0)
    {
        return;
    }

    PublishText("PLC", $"复位单点测试完成输出 M{PlcStepDoneBase}");
    for (int i = 0; i < count; i++)
    {
        ExecutePlcMCommand($"M{PlcStepDoneBase}=0");
        if (PlcCommandGapMs > 0)
        {
            await SleepAsync(PlcCommandGapMs);
        }
    }
}

async Task SendPlcStepDoneAsync(string stepName, int stepIndex)
{
    if (!PlcAutoTrigger || !PlcUseStepHandshake)
    {
        return;
    }

    string doneAddress = $"M{PlcStepDoneBase}";
    PublishText("PLC", $"发送[{stepName}]单点测试完成 {doneAddress}=1");
    ExecutePlcMCommand($"{doneAddress}=1");
    await SleepAsync(Math.Max(0, PlcCommandGapMs));

    if (PlcStepDonePulseMs > 0)
    {
        await SleepAsync(PlcStepDonePulseMs);
        PublishText("PLC", $"复位[{stepName}]单点测试完成 {doneAddress}=0");
        ExecutePlcMCommand($"{doneAddress}=0");
        await SleepAsync(Math.Max(0, PlcCommandGapMs));
    }
}

void EnsurePlcTestDoneIdle()
{
    string doneAddress = $"M{PlcTestDoneM}";
    bool value = ReadPlcM(doneAddress);
    if (!value)
    {
        PublishText("PLC", $"测试完成输出初始状态正常: {doneAddress}=0");
        return;
    }

    throw new InvalidOperationException($"测试启动前 PLC 测试完成点未复位: {doneAddress}=1");
}

void ExecutePlcMCommand(string token)
{
    if (!_plcConnected)
    {
        throw new InvalidOperationException("PLC 未连接");
    }

    string normalized = token.Trim();
    string[] parts = normalized.Split(new[] { '=' }, 2);
    if (parts.Length != 2)
    {
        throw new InvalidOperationException($"PLC 命令格式无效，仅支持 M 点位: {token}");
    }

    string address = parts[0].Trim().ToUpperInvariant();
    string rawValue = parts[1].Trim();
    if (!IsValidPlcMAddress(address) || !IsValidPlcBitValue(rawValue))
    {
        throw new InvalidOperationException($"PLC 命令格式无效，仅支持 M 点位: {token}");
    }

    if (address == $"M{PlcKeyDoneM}")
    {
        throw new InvalidOperationException($"PLC 完成返回位 {address} 只能读取，不能作为输出命令写入");
    }

    bool coilValue = ParsePlcBitValue(rawValue);
    PLC.Write(address, coilValue);
    PublishText("PLC命令", $"{address} = {(coilValue ? 1 : 0)}");
}

async Task WaitPlcKeyDoneAsync(string stepName)
{
    if (!_plcConnected)
    {
        throw new InvalidOperationException("PLC 未连接");
    }

    string doneAddress = $"M{PlcKeyDoneM}";
    DateTime deadline = DateTime.Now.AddMilliseconds(Math.Max(1, PlcKeyDoneTimeoutMs));
    bool seenDone = false;

    PublishText("PLC", $"等待[{stepName}]按键完成返回 {doneAddress}=1");

    while (DateTime.Now <= deadline)
    {
        ThrowIfStopRequested();

        bool done = ReadPlcM(doneAddress, PlcLogPollingReads);
        if (done)
        {
            seenDone = true;
            PublishText("PLC", $"[{stepName}]按键完成返回: {doneAddress}=1");
            break;
        }

        await SleepAsync(Math.Max(10, PlcKeyDonePollMs));
    }

    if (!seenDone)
    {
        throw new TimeoutException($"等待 PLC 按键完成返回超时: {doneAddress}");
    }
}

async Task WaitPlcPositionReadyAsync(string stepName, int stepIndex)
{
    if (!_plcConnected)
    {
        throw new InvalidOperationException("PLC 未连接");
    }

    string readyAddress = $"M{PlcPositionReadyBase + stepIndex}";
    DateTime deadline = DateTime.Now.AddMilliseconds(Math.Max(1, PlcPositionReadyTimeoutMs));

    PublishText("PLC", $"等待[{stepName}]位置到位返回 {readyAddress}=1");

    while (DateTime.Now <= deadline)
    {
        ThrowIfStopRequested();

        if (ReadPlcM(readyAddress, PlcLogPollingReads))
        {
            PublishText("PLC", $"[{stepName}]位置到位返回: {readyAddress}=1");
            return;
        }

        await SleepAsync(Math.Max(10, PlcPositionReadyPollMs));
    }

    throw new TimeoutException($"等待 PLC 位置到位返回超时: {readyAddress}");
}

async Task WaitPlcKeyDoneResetAsync(string stepName)
{
    string doneAddress = $"M{PlcKeyDoneM}";
    DateTime resetDeadline = DateTime.Now.AddMilliseconds(PlcKeyDoneResetTimeoutMs);

    PublishText("PLC", $"等待[{stepName}]按键完成返回复位 {doneAddress}=0");

    while (DateTime.Now <= resetDeadline)
    {
        ThrowIfStopRequested();

        if (!ReadPlcM(doneAddress, PlcLogPollingReads))
        {
            PublishText("PLC", $"[{stepName}]按键完成返回已复位: {doneAddress}=0");
            return;
        }

        await SleepAsync(Math.Max(10, PlcKeyDonePollMs));
    }

    throw new TimeoutException($"PLC 按键完成返回未复位: {doneAddress}");
}

async Task EnsurePlcKeyDoneIdleAsync(string stepName)
{
    string doneAddress = $"M{PlcKeyDoneM}";
    if (!ReadPlcM(doneAddress))
    {
        return;
    }

    PublishText("PLC", $"[{stepName}]开始前检测到完成位仍为 1，等待复位 {doneAddress}");
    await WaitPlcKeyDoneResetAsync(stepName);
}

async Task EnsurePlcStepDoneIdleAsync(string stepName, int stepIndex)
{
    if (!PlcUseStepHandshake)
    {
        return;
    }

    string doneAddress = $"M{PlcStepDoneBase}";
    if (!ReadPlcM(doneAddress))
    {
        return;
    }

    PublishText("PLC", $"[{stepName}]开始前检测到单点完成位仍为 1，先复位 {doneAddress}=0");
    ExecutePlcMCommand($"{doneAddress}=0");
    await SleepAsync(Math.Max(0, PlcCommandGapMs));
}

bool ReadPlcM(string address, bool logRead = true)
{
    bool[] values = PLC.Read(address, 1);
    bool value = values != null && values.Length > 0 && values[0];
    if (logRead)
    {
        PublishText("PLC读取", $"{address} = {(value ? 1 : 0)}");
    }
    return value;
}

async Task SleepAsync(int milliseconds)
{
    int remainingMs = Math.Max(0, milliseconds);
    while (remainingMs > 0)
    {
        ThrowIfStopRequested();
        int chunkMs = Math.Min(remainingMs, 100);
        await Task.Delay(chunkMs, GetExecutionToken());
        remainingMs -= chunkMs;
    }
}

bool IsValidPlcMAddress(string address)
{
    if (string.IsNullOrWhiteSpace(address) || address.Length < 2 || address[0] != 'M')
    {
        return false;
    }

    for (int i = 1; i < address.Length; i++)
    {
        if (!char.IsDigit(address[i]))
        {
            return false;
        }
    }

    return true;
}

bool IsValidPlcBitValue(string text)
{
    string upper = (text ?? "").Trim().ToUpperInvariant();
    return upper == "0" || upper == "1" || upper == "ON" || upper == "OFF" ||
        upper == "TRUE" || upper == "FALSE" || upper == "SET" || upper == "RESET";
}

bool ParsePlcBitValue(string text)
{
    string upper = (text ?? "").Trim().ToUpperInvariant();
    return upper == "1" || upper == "ON" || upper == "TRUE" || upper == "SET";
}

async Task<string> SendShellCommand(string command, int timeoutMs)
{
    if (_shellHandle == null || !_shellConnected)
    {
        throw new InvalidOperationException("BLE Shell 会话未建立");
    }

    ProcessAsync.ClearOutputBuffer(_shellHandle);
    ProcessAsync.ClearErrorBuffer(_shellHandle);
    PublishText("Shell命令", $"V3> {command}");

    return await ProcessAsync.SendCommandAndReadAsync(
        _shellHandle,
        command,
        "V3>",
        timeoutMs,
        GetExecutionToken());
}

async Task ExitTestModeIfNeeded()
{
    if (!_shellConnected || !_enteredTestMode)
    {
        return;
    }

    string output = await SendShellCommand("exit-test", BleCommandTimeoutMs);
    PublishText("BLE输出", output);
    _enteredTestMode = false;
}

void CloseShell()
{
    if (_shellHandle == null)
    {
        _shellConnected = false;
        _enteredTestMode = false;
        return;
    }

    try
    {
        if (_shellConnected)
        {
            try
            {
                ProcessAsync.SendCommand(_shellHandle, "quit");
                WaitShellExitAfterQuit();
            }
            catch (Exception ex)
            {
                PublishText("系统", $"发送 quit 关闭 BLE Shell 异常: {ex.Message}");
            }
        }

        try { ProcessAsync.ClearOutputBuffer(_shellHandle); } catch { }
        try { ProcessAsync.ClearErrorBuffer(_shellHandle); } catch { }
        try { ProcessAsync.Close(_shellHandle); } catch { }
    }
    finally
    {
        _shellHandle = null;
        _shellConnected = false;
        _enteredTestMode = false;
    }
}

void RecordResults()
{
    PublishText("结果", "━━━━━ 测试结果 ━━━━━");
    PublishValue("事件通过数", _eventPassedCount, "", _steps.Count, _steps.Count);
    int capTargetCount = EnableCapValidation ? _steps.Count(step => step.CapId.HasValue) : 0;
    PublishValue("电容通过数", _capPassedCount, "", capTargetCount, capTargetCount);

    foreach (var kvp in _stepCapSignals.OrderBy(item => item.Key))
    {
        PublishText("结果", $"{kvp.Key} 电容值 = {kvp.Value}");
    }

    _testPassed = Context != null && Context.Status == TestStatus.Passed;
    PublishState("测试结果", _testPassed);
    PublishText("结果", _testPassed ? "PASS" : "FAIL");
}

void MESgetsfckeybysfc()
{
    string failmessage = "";
    bool result = _mes.MES_getsfckey_bysfc(Context.SerialNumber, ref failmessage, ref sfcKeyList);
    PublishText("MES", $"桩号绑定结果: {result}, 消息: {failmessage}");

    string targetKey = "主板";
    var keyItem = sfcKeyList.FirstOrDefault(item => item.name == targetKey);
    if (result && keyItem != null)
    {
        _pcbaSn = keyItem.value;
        PublishValue("主板条码", _pcbaSn, true);
        PublishText("MES", $"主板条码: {_pcbaSn}");
    }
    else
    {
        PublishValue("主板条码", "", false);
    }
}

void MesStart()
{
    if (!MESEnble)
    {
        PublishValue("MES启动", "跳过MES", true);
        return;
    }

    string failmessage = "";
    bool ok = _mes.MesStart(Context.SerialNumber, out failmessage);
    PublishText("MES", ok ? "MesStart 成功" : $"MesStart 失败: {failmessage}");
    PublishValue("MES启动", "", ok);
}

string GetMesFailReason()
{
    if (!string.IsNullOrWhiteSpace(Context?.LastException?.Message))
    {
        return Context.LastException.Message;
    }

    return "TEST_FAIL";
}

void MesComplete()
{
    if (!MESEnble)
    {
        _mesFinalSummary = "MES跳过";
        PublishValue("MES完成上报", "跳过MES", true);
        return;
    }

    string failmessage = "";
    bool testPassed = Context != null && Context.Status == TestStatus.Passed;
    if (testPassed)
    {
        bool ok = _mes.MesComplete(Context.SerialNumber, "", out failmessage);
        _mesFinalSummary = ok ? "MES完成上报成功" : $"MES完成上报失败: {failmessage}";
        PublishText("MES", ok ? "MesComplete(PASS) 成功" : $"MesComplete 失败: {failmessage}");
        PublishState("MES完成上报", ok);
        return;
    }

    string mesFailReason = GetMesFailReason();
    bool okFail = _mes.MesNCComplete(Context.SerialNumber, mesFailReason, out failmessage);
    _mesFinalSummary = okFail ? "MESNC上报成功" : $"MESNC上报失败: {failmessage}";
    PublishText("MES", okFail ? "MesNCComplete(FAIL) 成功" : $"MesNCComplete 失败: {failmessage}");
    PublishState("MESNC上报", okFail);
}

async Task OnTestFinishedAsync()
{

    try
    {
        try
        {
            MesComplete();
        }
        catch (Exception ex)
        {
            _mesFinalSummary = $"MES上报异常: {ex.Message}";
            PublishText("MES", _mesFinalSummary);
        }

        if (_shellConnected && _enteredTestMode)
        {
            try
            {
                await ExitTestModeIfNeeded();
            }
            catch (Exception ex)
            {
                PublishText("系统", $"退出产测模式异常: {ex.Message}");
            }
        }
    }
    finally
    {
        ClosePlcClient();
        CloseShell();
        ResetCurrentSerialNumberInput();
        PublishText("系统", string.IsNullOrWhiteSpace(_mesFinalSummary) ? "清理完成" : $"清理完成 | {_mesFinalSummary}");
    }
}

void WaitShellExitAfterQuit()
{
    int waitMs = Math.Max(0, BleShellQuitWaitMs);
    if (waitMs <= 0 || _shellHandle == null)
    {
        return;
    }

    DateTime deadline = DateTime.Now.AddMilliseconds(waitMs);
    while (DateTime.Now < deadline)
    {
        try
        {
            if (_shellHandle.HasExited)
            {
                return;
            }
        }
        catch
        {
            return;
        }

        Thread.Sleep(100);
    }

    try
    {
        if (!_shellHandle.HasExited)
        {
            PublishText("系统", $"BLE Shell 未在 {waitMs} ms 内退出，将强制关闭进程");
        }
    }
    catch
    {
    }
}
