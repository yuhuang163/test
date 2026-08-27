// ============================================
// V3 light sample calibration by golden currents Rev1.0
//
// Purpose:
// 1. Do not use the TES1339R lux meter.
// 2. Use LightSource_CurrentList produced by the golden-sample current calibration script.
// 3. Use LightCal_GoldenSampleList produced by the golden-sample current calibration script.
// 4. Read DUT raw light values at each current.
// 5. Write set-light-calib index offset, where offset = dutRaw - goldenRaw.
// ============================================

CancellationToken GetExecutionToken() => Context?.CancellationToken ?? CancellationToken.None;
bool IsStopRequested() => GetExecutionToken().IsCancellationRequested;


ProcessAsyncFunctions.ProcessAsync _processAsync = new ProcessAsyncFunctions.ProcessAsync();
ProcessAsyncFunctions.IProcessHandle _shellHandle = null;
bool _shellConnected = false;
bool _enteredTestMode = false;
bool _lightReportEnabled = false;

SerialPortFunction.SerialPortClient _lightSourceHandle = null;
bool _lightSourceConnected = false;

// ==================== MES ====================
BYDMes _mes = new BYDMes();
List<BYDMES.SfcKeyInfo> sfcKeyList = new List<BYDMES.SfcKeyInfo>();
string _mesFinalSummary = "";

string _pcbaSn = "";
string _deviceMac = "";
int _currentLightCurrent = 0;
List<string> _summary = new List<string>();
List<int> _dutValues = new List<int>();

string ProductModel => Param<string>("ProductModel", "V3");

string V3ExePath => Param<string>("V3_ExePath", @"C:\bat\v3_exe.bat");
int BleConnectMode => Param<int>("BLE_ConnectMode", 1); // 1=SN MAC, 2=param MAC, 3=auto strongest
string BleDeviceMac => Param<string>("BLE_DeviceMac", "");
int BleConnectTimeoutMs => Param<int>("BLE_ConnectTimeoutMs", 15000);
int BleCommandTimeoutMs => Param<int>("BLE_CommandTimeoutMs", 10000);
bool BleEnterTestMode => Param<bool>("BLE_EnterTestMode", true);
int MacStartIndex => Param<int>("MacStartIndex", 4);
int MacLength => Param<int>("MacLength", 12);
bool MESEnble => Param<bool>("MESEnble", true);
string MESBoardKeyName => Param<string>("MES_BoardKeyName", "主板");
bool DisablePanelLights => Param<bool>("LightCal_DisablePanelLights", true);
bool RequirePanelLightsOff => Param<bool>("LightCal_RequirePanelLightsOff", true);
string ScreenLightOffCommand => Param<string>("LightCal_ScreenLightOffCommand", "lcd-off");
string KeyLightOffCommand => Param<string>("LightCal_KeyLightOffCommand", "key-light-off");
bool RestorePanelLightsAtEnd => Param<bool>("LightCal_RestorePanelLightsAtEnd", true);
string ScreenLightOnCommand => Param<string>("LightCal_ScreenLightOnCommand", "lcd-on");
string KeyLightOnCommand => Param<string>("LightCal_KeyLightOnCommand", "key-light-on");

string LightSourceCurrentList => Param<string>(
    "LightSource_CurrentList",
    Param<string>("LightSource_BrightnessList", "0,6,8,10,12,15,21,26,48,70,92,104,109"));
string GoldenSampleList => Param<string>("LightCal_GoldenSampleList", "1,38,76,113,148,202,310,399,791,1153,1489,1620,1637");
string TargetLuxList => Param<string>("LightCal_TargetLuxList", "0,10,20,30,40,50,80,100,200,300,400,500,600");
int CalibBaseIndex => Param<int>("LightCal_BaseIndex", 0);
bool WriteCalibration => Param<bool>("LightCal_WriteCalibration", true);
bool VerifyReadback => Param<bool>("LightCal_VerifyReadback", true);
bool RequireIncreasing => Param<bool>("LightCal_RequireIncreasing", false);
int MinResponseDelta => Param<int>("LightCal_MinResponseDelta", 0);
int MaxAbsOffsetRaw => Param<int>("LightCal_MaxAbsOffsetRaw", 0);
int ReadRepeatCount => Param<int>("LightCal_ReadRepeatCount", 3);

int LightSettleMs => Param<int>("LightCal_SettleMs", 1000);
int WarmupPacketCount => Param<int>("LightCal_WarmupPacketCount", 0);
int WaitLightTimeoutMs => Param<int>("LightCal_WaitLightTimeoutMs", 5000);
bool ResetLightReportEachPoint => Param<bool>("LightCal_ResetReportEachPoint", true);
bool UseStableTailSamples => Param<bool>("LightCal_UseStableTailSamples", true);
int StableTailSampleCount => Param<int>("LightCal_StableTailSampleCount", 5);

string LightSourceComPort => Param<string>("LightSource_ComPort", "COM8");
int LightSourceBaudRate => Param<int>("LightSource_BaudRate", 9600);
int LightSourceChannel => Param<int>("LightSource_Channel", 1);
int LightSourceOffCurrent => Param<int>("LightSource_OffCurrent", 0);
bool LightSourceTurnOffAtEnd => Param<bool>("LightSource_TurnOffAtEnd", true);

[UIAction("test.start")]
[UIAction("input.serialnumber")]
async Task MainTest(string serialnumber)
{
    Context.SerialNumber = serialnumber?.Trim() ?? "";
    ResetState();

    _pcbaSn = Context.SerialNumber;
    MesStart();
    MESgetsfckeybysfc();
    PublishText("PCBA SN", _pcbaSn);

    try
    {
        if (BleConnectMode == 1)
        {
            if (!TryParseMacFromPcbaSn(_pcbaSn, out _deviceMac))
            {
                throw new Exception("BLE_ConnectMode=1 时要求条码中包含可解析的 MAC");
            }
        }

        await ExecuteSampleCalibration();
    }
    catch (Exception ex)
    {
        FailTest("V3光感金样电流样品校准", ex.Message);
        PublishText("错误", ex.Message);
        PublishAlarm("样品校准失败", "error");
        PublishStatus("状态", "失败");
    }
}

[UIAction("test.stop")]
void StopTest()
{
    PublishText("系统", "停止测试");
    CloseShell();
    DisconnectLightSource();
    PublishStatus("状态", "已停止");
}

[UIAction("MESTurnOn")]
public void OnSwitchToggled(bool isOn) => SetParam("MESEnble", isOn);

async Task ExecuteSampleCalibration()
{
    List<int> currents = ParseIntList(LightSourceCurrentList, "LightSource_CurrentList");
    List<int> goldenRaws = ParseIntList(GoldenSampleList, "LightCal_GoldenSampleList");
    List<double> targetLabels = ParseDoubleListOrEmpty(TargetLuxList);
    ValidateLayout(currents, goldenRaws);

    try
    {
        PublishStatus("状态", "运行中");
        PublishProgress("测试进度", 0, "开始");
        PublishText("测试状态", "━━━━━ V3 光感金样电流样品校准 ━━━━━");
        PublishText("测试状态", "当前脚本不使用照度计，按金样电流列表读取待测品并写入光感偏移。");
        PublishValue("测试条码", SerialNumber, true);

        ShowConfiguration(currents, goldenRaws, targetLabels);

        InitializeLightSource();
        if (IsStopRequested()) return;

        await ConnectBleShell();
        if (IsStopRequested()) return;

        if (BleEnterTestMode)
        {
            await EnterTestMode();
            if (IsStopRequested()) return;
        }

        await DisablePanelLightsIfNeeded();
        if (IsStopRequested()) return;

        if (!ResetLightReportEachPoint)
        {
            await EnableLightReport();
            if (IsStopRequested()) return;
        }

        for (int i = 0; i < currents.Count; i++)
        {
            if (IsStopRequested()) return;
            double? label = i < targetLabels.Count ? targetLabels[i] : (double?)null;
            await RunCalibrationPoint(i, currents[i], goldenRaws[i], label, currents.Count);
        }

        VerifyResponseTrend();
        PublishSummary();

        PublishState("测试结果", true);
        PublishStatus("状态", "完成");
        PublishProgress("测试进度", 100, "完成");
    }
    finally
    {
        await DisableLightReportIfNeeded();
        await RestorePanelLightsIfNeeded();
        await ExitTestModeIfNeeded();
        CloseShell();
        TurnOffLightSourceIfNeeded();
        DisconnectLightSource();
    }
}

void ShowConfiguration(List<int> currents, List<int> goldenRaws, List<double> targetLabels)
{
    PublishText("配置", "━━━━━ 参数配置 ━━━━━");
    PublishText("配置", $"产品型号 = {ProductModel}");
    PublishText("配置", $"BLE连接模式 = {BleConnectMode} ({GetBleConnectModeName()})");
    PublishText("配置", $"目标MAC = {(string.IsNullOrWhiteSpace(_deviceMac) ? "AUTO/参数模式" : _deviceMac)}");
    PublishText("配置", $"BLE连接超时 = {BleConnectTimeoutMs} ms");
    PublishText("配置", $"BLE命令超时 = {BleCommandTimeoutMs} ms");
    PublishText("配置", $"进入产测模式 = {BleEnterTestMode}");
    PublishText("配置", $"关闭屏幕灯/按键灯 = {DisablePanelLights}, 严格校验 = {RequirePanelLightsOff}");
    PublishText("配置", $"屏幕灯关闭指令 = {ScreenLightOffCommand}, 按键灯关闭指令 = {KeyLightOffCommand}");
    PublishText("配置", $"结束恢复屏幕灯/按键灯 = {RestorePanelLightsAtEnd}, 屏幕灯打开指令 = {ScreenLightOnCommand}, 按键灯打开指令 = {KeyLightOnCommand}");
    PublishText("配置", $"光源控制器 = {LightSourceComPort}@{LightSourceBaudRate}, CH{LightSourceChannel}");
    PublishText("配置", $"光源电流 = {string.Join(", ", currents)}");
    PublishText("配置", $"金样基准 = {string.Join(", ", goldenRaws)}");
    PublishText("配置", $"照度标签 = {(targetLabels.Count == 0 ? "<未配置>" : string.Join(", ", targetLabels.Select(v => v.ToString("0.##"))))}");
    PublishText("配置", $"写入校准 = {WriteCalibration}");
    PublishText("配置", $"回读校验 = {VerifyReadback}");
    PublishText("配置", $"偏移fail阈值 = {(MaxAbsOffsetRaw > 0 ? $"|dut-golden| > {MaxAbsOffsetRaw} raw" : "未启用")}");
    PublishText("配置", $"校准索引 = {CalibBaseIndex}..{CalibBaseIndex + currents.Count - 1}");
    PublishText("配置", $"光源稳定等待 = {LightSettleMs} ms");
    PublishText("配置", $"wait-light 超时 = {WaitLightTimeoutMs} ms");
    PublishText("配置", $"预热包数 = {WarmupPacketCount}");
    PublishText("配置", $"单点读取次数 = {ReadRepeatCount}");
    PublishText("配置", $"每点重置光感上报 = {ResetLightReportEachPoint}");
    PublishText("配置", $"使用尾部稳定样本 = {UseStableTailSamples}, 尾部数量 = {StableTailSampleCount}");
}

void InitializeLightSource()
{
    PublishText("外设", "━━━━━ 初始化光源控制器 ━━━━━");
    PublishProgress("测试进度", 5, "初始化光源");

    string message;
    bool ok = ConnectLightSource(LightSourceComPort, LightSourceBaudRate, out message);
    PublishText("光源", message);
    PublishValue("光源连接", message, ok);
    if (!ok)
    {
        throw new Exception($"光源控制器初始化失败: {message}");
    }
}

async Task RunCalibrationPoint(int pointIndex, int current, int goldenRaw, double? targetLabel, int total)
{
    PublishText("校准", $"━━━━━ 点位 {pointIndex + 1}/{total} ━━━━━");
    PublishProgress("测试进度", 10 + (int)Math.Round((pointIndex * 75.0) / Math.Max(1, total)), $"点位{pointIndex + 1}");
    if (targetLabel.HasValue)
    {
        PublishText("校准", $"照度标签 = {targetLabel.Value:0.##} lux");
        PublishValue($"目标照度[{pointIndex + 1}]", targetLabel.Value, "lux");
    }

    if (ResetLightReportEachPoint)
    {
        await DisableLightReportIfNeeded();
    }

    PublishText("光源", $"设置 CH{LightSourceChannel} 电流 = {current}");
    string message;
    bool ok = SetLightSourceSingleChannel(LightSourceChannel, current, out message);
    PublishText("光源", message);
    PublishValue($"光源电流[{pointIndex + 1}]", current, "level");
    if (!ok)
    {
        throw new Exception($"设置光源失败: {message}");
    }

    _currentLightCurrent = current;
    if (LightSettleMs > 0)
    {
        await SleepAsync(LightSettleMs);
    }

    try
    {
        if (ResetLightReportEachPoint)
        {
            await EnableLightReport();
        }

        int dutRaw = await AcquireRepeatedProductLightValue(pointIndex);
        int offset = dutRaw - goldenRaw;
        int calibIndex = CalibBaseIndex + pointIndex;

        PublishValue($"金样[{pointIndex + 1}]", goldenRaw, "raw");
        PublishValue($"待测品[{pointIndex + 1}]", dutRaw, "raw");
        PublishValue($"偏移[{pointIndex + 1}]", offset, "raw");
        _dutValues.Add(dutRaw);

        if (MaxAbsOffsetRaw > 0 && Math.Abs(offset) > MaxAbsOffsetRaw)
        {
            throw new Exception($"光感偏移超限: point={pointIndex + 1}, dut={dutRaw}, golden={goldenRaw}, offset={offset}, limit=±{MaxAbsOffsetRaw} raw");
        }

        if (WriteCalibration)
        {
            PublishText("校准", $"写入 set-light-calib {calibIndex} {offset} (dut={dutRaw} - golden={goldenRaw})");
            await WriteCalibrationValue(calibIndex, offset, "光感偏移");
        }
        else
        {
            PublishText("校准", $"未写入，仅计算 offset={offset}, index={calibIndex}");
        }

        string line = $"点位{pointIndex + 1}: current={current}, golden={goldenRaw}, dut={dutRaw}, offset={offset}, index={calibIndex}";
        PublishText("校准结果", line);
        _summary.Add(line);
    }
    finally
    {
        if (ResetLightReportEachPoint)
        {
            await DisableLightReportIfNeeded();
        }
    }
}

async Task<int> AcquireRepeatedProductLightValue(int pointIndex)
{
    List<int> repeated = new List<int>();
    int repeatCount = Math.Max(1, ReadRepeatCount);
    for (int i = 0; i < repeatCount; i++)
    {
        int raw = await AcquireProductLightValue(pointIndex, i);
        repeated.Add(raw);
    }

    int rawValue = MedianInt(repeated);
    PublishText("产品光感", $"点位{pointIndex + 1} 最终中位数 = {rawValue}, readings=[{string.Join(", ", repeated)}]");
    return rawValue;
}

async Task<int> AcquireProductLightValue(int pointIndex, int repeatIndex)
{
    for (int i = 0; i < Math.Max(0, WarmupPacketCount); i++)
    {
        string warmupOutput = await SendShellCommand($"wait-light {WaitLightTimeoutMs}", Math.Max(BleCommandTimeoutMs, WaitLightTimeoutMs + 3000));
        PublishText("产品光感预热", warmupOutput);
    }

    string output = await SendShellCommand($"wait-light {WaitLightTimeoutMs}", Math.Max(BleCommandTimeoutMs, WaitLightTimeoutMs + 3000));
    PublishText("产品光感输出", output);

    var values = ParseLightValues(output);
    if (values.Count == 0)
    {
        throw new Exception($"无法解析 wait-light 返回值: {output}");
    }

    var stableValues = SelectStableValues(values);
    int median = MedianInt(stableValues);
    int min = values.Min();
    int max = values.Max();

    PublishText("产品光感", $"点位{pointIndex + 1}, 第{repeatIndex + 1}次, samples=[{string.Join(", ", values)}], stable=[{string.Join(", ", stableValues)}], median={median}, min={min}, max={max}");
    return median;
}

List<int> SelectStableValues(List<int> values)
{
    if (!UseStableTailSamples || values.Count == 0)
    {
        return values;
    }

    int count = Math.Max(1, Math.Min(StableTailSampleCount, values.Count));
    return values.Skip(values.Count - count).ToList();
}

int MedianInt(List<int> values)
{
    if (values == null || values.Count == 0)
    {
        throw new Exception("无法计算中位数：样本为空");
    }

    var sorted = values.OrderBy(v => v).ToList();
    int middle = sorted.Count / 2;
    if (sorted.Count % 2 == 1)
    {
        return sorted[middle];
    }

    return (int)Math.Round((sorted[middle - 1] + sorted[middle]) / 2.0);
}

List<int> ParseLightValues(string output)
{
    string text = output ?? "";
    int labelIndex = text.IndexOf("Lux Values:", StringComparison.OrdinalIgnoreCase);
    if (labelIndex < 0)
    {
        return new List<int>();
    }

    int start = text.IndexOf('[', labelIndex);
    int end = start >= 0 ? text.IndexOf(']', start + 1) : -1;
    if (start < 0 || end <= start)
    {
        return new List<int>();
    }

    return text.Substring(start + 1, end - start - 1)
        .Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries)
        .Select(s => s.Trim())
        .Where(s => !string.IsNullOrWhiteSpace(s))
        .Select(int.Parse)
        .ToList();
}

async Task WriteCalibrationValue(int index, int value, string label)
{
    string setOutput = await SendShellCommand($"set-light-calib {index} {value}", BleCommandTimeoutMs);
    PublishText($"{label}写入", setOutput);
    bool setOk = setOutput.Contains("[OK]");

    if (!setOk)
    {
        throw new Exception($"{label}写入失败: index={index}, value={value}, output={setOutput}");
    }

    if (!VerifyReadback)
    {
        return;
    }

    string readOutput = await SendShellCommand($"get-light-calib {index}", BleCommandTimeoutMs);
    PublishText($"{label}回读", readOutput);
    if (!TryParseLastIntegerAfterColon(readOutput, out int readValue))
    {
        throw new Exception($"{label}回读解析失败: index={index}, output={readOutput}");
    }

    PublishValue($"{label}回读[{index}]", readValue, "", value, value);

    if (readValue != value)
    {
        throw new Exception($"{label}回读不一致: index={index}, write={value}, read={readValue}");
    }
}

bool TryParseLastIntegerAfterColon(string text, out int value)
{
    value = 0;
    if (string.IsNullOrWhiteSpace(text))
    {
        return false;
    }

    int colonIndex = text.LastIndexOf(':');
    string tail = colonIndex >= 0 ? text.Substring(colonIndex + 1) : text;
    tail = tail.Trim();

    int start = 0;
    while (start < tail.Length && char.IsWhiteSpace(tail[start]))
    {
        start++;
    }

    int end = start;
    if (end < tail.Length && tail[end] == '-')
    {
        end++;
    }

    while (end < tail.Length && char.IsDigit(tail[end]))
    {
        end++;
    }

    return end > start && int.TryParse(tail.Substring(start, end - start), out value);
}

void VerifyResponseTrend()
{
    if (_dutValues.Count == 0)
    {
        throw new Exception("没有产品光感数据，无法判断响应情况");
    }

    int delta = _dutValues[_dutValues.Count - 1] - _dutValues[0];
    PublishValue("产品光感首尾差", delta, "raw");

    if (MinResponseDelta > 0 && Math.Abs(delta) < MinResponseDelta)
    {
        throw new Exception($"产品光感响应差过小: delta={delta}, min={MinResponseDelta}");
    }

    if (!RequireIncreasing)
    {
        return;
    }

    for (int i = 1; i < _dutValues.Count; i++)
    {
        if (_dutValues[i] < _dutValues[i - 1])
        {
            throw new Exception($"产品光感未递增: point{i}={_dutValues[i - 1]}, point{i + 1}={_dutValues[i]}");
        }
    }
}

void PublishSummary()
{
    PublishProgress("测试进度", 95, "汇总");
    PublishText("结果", "━━━━━ 光感金样电流样品校准汇总 ━━━━━");
    for (int i = 0; i < _summary.Count; i++)
    {
        PublishText("结果", _summary[i]);
    }

    PublishText("结果", "V3 光感金样电流样品校准完成");
}

List<int> ParseIntList(string text, string name)
{
    var values = (text ?? "")
        .Split(new[] { ',', ';', '|', '、', '\n', '\r' }, StringSplitOptions.RemoveEmptyEntries)
        .Select(s => s.Trim())
        .Where(s => !string.IsNullOrWhiteSpace(s))
        .Select(int.Parse)
        .ToList();

    if (values.Count == 0)
    {
        throw new Exception($"{name} 为空");
    }

    return values;
}

List<double> ParseDoubleListOrEmpty(string text)
{
    return (text ?? "")
        .Split(new[] { ',', ';', '|', '、', '\n', '\r' }, StringSplitOptions.RemoveEmptyEntries)
        .Select(s => s.Trim())
        .Where(s => !string.IsNullOrWhiteSpace(s))
        .Select(s => double.Parse(s, System.Globalization.CultureInfo.InvariantCulture))
        .ToList();
}

void ValidateLayout(List<int> currents, List<int> goldenRaws)
{
    if (currents.Count != goldenRaws.Count)
    {
        throw new Exception($"LightSource_CurrentList 数量({currents.Count})与 LightCal_GoldenSampleList 数量({goldenRaws.Count})不一致");
    }

    int calibEnd = CalibBaseIndex + currents.Count - 1;
    if (CalibBaseIndex < 0 || calibEnd > 12)
    {
        throw new Exception($"校准索引越界: {CalibBaseIndex}..{calibEnd}, 当前规则要求 0..12");
    }

    for (int i = 0; i < currents.Count; i++)
    {
        if (currents[i] < 0 || currents[i] > 255)
        {
            throw new Exception($"光源电流越界: index={i + 1}, value={currents[i]}, allowed=0..255");
        }
    }
}

string GetBleConnectModeName()
{
    return BleConnectMode switch
    {
        1 => "条码解析MAC",
        2 => "参数指定MAC",
        3 => "自动最强信号",
        _ => "未知"
    };
}

bool TryNormalizeMac(string input, out string macAddress)
{
    macAddress = "";
    if (string.IsNullOrWhiteSpace(input))
    {
        return false;
    }

    string hex = input.Replace(":", "").Replace("-", "").Trim().ToUpperInvariant();
    if (hex.Length != 12 || hex.Any(c => !((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))))
    {
        return false;
    }

    macAddress = string.Join(":", Enumerable.Range(0, 6).Select(i => hex.Substring(i * 2, 2)));
    return true;
}

string BuildBleConnectArguments()
{
    if (BleConnectMode == 1)
    {
        if (string.IsNullOrWhiteSpace(_deviceMac) && !TryParseMacFromPcbaSn(_pcbaSn, out _deviceMac))
        {
            throw new InvalidOperationException("连接模式1要求从条码解析MAC，但当前PCBA SN格式无效");
        }

        PublishText("MAC解析", $"从 SN 解析 MAC: {_deviceMac}");
        return $"--mac {_deviceMac}";
    }

    if (BleConnectMode == 2)
    {
        if (!TryNormalizeMac(BleDeviceMac, out _deviceMac))
        {
            throw new InvalidOperationException("连接模式2要求参数 BLE_DeviceMac 为有效MAC");
        }

        PublishText("MAC解析", $"使用参数 MAC: {_deviceMac}");
        return $"--mac {_deviceMac}";
    }

    if (BleConnectMode == 3)
    {
        _deviceMac = "";
        PublishText("BLE", "连接模式3：不指定MAC，自动扫描并连接最强信号设备");
        return "";
    }

    throw new InvalidOperationException($"不支持的 BLE_ConnectMode: {BleConnectMode}");
}

async Task ConnectBleShell()
{
    string arguments = BuildBleConnectArguments();

    PublishText("蓝牙", "━━━━━ BLE连接 ━━━━━");
    PublishProgress("测试进度", 8, "BLE连接");
    PublishText("BLE", $"ConnectMode={BleConnectMode} ({GetBleConnectModeName()}), Target={(string.IsNullOrWhiteSpace(_deviceMac) ? "AUTO" : _deviceMac)}, Args={(string.IsNullOrWhiteSpace(arguments) ? "<empty>" : arguments)}");
    _processAsync.SetArgumentsAndWorkingDirectory(arguments, null);
    _shellHandle = _processAsync.Create(V3ExePath, ProcessAsyncFunctions.ProcessWindowStyleOption.Hidden);

    string output = await ProcessAsyncFunctions.ProcessAsync.SendCommandAndReadRegexAsync(
        _shellHandle,
        "",
        @"(V3>|Connected|ERROR|FAIL|失败)",
        BleConnectTimeoutMs,
        GetExecutionToken());

    PublishText("蓝牙输出", output);

    if (output.Contains("V3>") || output.Contains("Connected") || output.Contains("连接成功"))
    {
        _shellConnected = true;
        PublishText("蓝牙", $"已连接设备: {(string.IsNullOrWhiteSpace(_deviceMac) ? "AUTO" : _deviceMac)}");
        PublishValue("BLE_CONNECT", "", true);
        await SleepAsync(200);
        return;
    }

    PublishValue("BLE_CONNECT", "", false);
    throw new Exception($"BLE连接失败: {output}");
}

async Task EnterTestMode()
{
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

async Task DisablePanelLightsIfNeeded()
{
    if (!DisablePanelLights)
    {
        PublishText("光感环境", "跳过关闭屏幕灯/按键灯");
        return;
    }

    PublishText("光感环境", "关闭屏幕灯和按键灯，避免影响光感校准");
    await SendPanelLightOffCommand("屏幕灯", ScreenLightOffCommand);
    await SendPanelLightOffCommand("按键灯", KeyLightOffCommand);
}

async Task SendPanelLightOffCommand(string label, string command)
{
    if (string.IsNullOrWhiteSpace(command))
    {
        PublishText("光感环境", $"{label}关闭指令为空，跳过");
        return;
    }

    string output = await SendShellCommand(command.Trim(), BleCommandTimeoutMs);
    PublishText($"{label}关闭输出", output);
    bool ok = output.Contains("[OK]") || output.Contains("success", StringComparison.OrdinalIgnoreCase) || output.Contains("成功");
    PublishValue($"{label}关闭", command, ok);
    if (!ok && RequirePanelLightsOff)
    {
        throw new Exception($"{label}关闭失败: command={command}, output={output}");
    }
}

async Task RestorePanelLightsIfNeeded()
{
    if (!DisablePanelLights || !RestorePanelLightsAtEnd || !_shellConnected)
    {
        return;
    }

    PublishText("光感环境", "恢复屏幕灯和按键灯");
    await SendPanelLightRestoreCommand("按键灯", KeyLightOnCommand);
    await SendPanelLightRestoreCommand("屏幕灯", ScreenLightOnCommand);
}

async Task SendPanelLightRestoreCommand(string label, string command)
{
    if (string.IsNullOrWhiteSpace(command))
    {
        PublishText("光感环境", $"{label}打开指令为空，跳过");
        return;
    }

    try
    {
        string output = await SendShellCommand(command.Trim(), BleCommandTimeoutMs);
        PublishText($"{label}打开输出", output);
        bool ok = output.Contains("[OK]") || output.Contains("success", StringComparison.OrdinalIgnoreCase) || output.Contains("成功");
        PublishValue($"{label}打开", command, ok);
        if (!ok)
        {
            PublishAlarm($"{label}打开失败: command={command}", "warning");
        }
    }
    catch (Exception ex)
    {
        PublishText("光感环境", $"{label}打开异常: {ex.Message}");
        PublishAlarm($"{label}打开异常", "warning");
    }
}

async Task EnableLightReport()
{
    PublishText("光感", "开启光感持续上报");
    string output = await SendShellCommand("light-report-on", BleCommandTimeoutMs);
    PublishText("光感输出", output);

    if (output.Contains("[OK]"))
    {
        _lightReportEnabled = true;
        PublishValue("LIGHT_REPORT_ON", "", true);
        await SleepAsync(100);
        return;
    }

    PublishValue("LIGHT_REPORT_ON", "", false);
    throw new Exception($"开启光感上报失败: {output}");
}

async Task DisableLightReportIfNeeded()
{
    if (!_shellConnected || !_lightReportEnabled)
    {
        return;
    }

    try
    {
        string output = await SendShellCommand("light-report-off", BleCommandTimeoutMs);
        PublishText("光感输出", output);
        PublishValue("LIGHT_REPORT_OFF", "", output.Contains("[OK]"));
    }
    catch (Exception ex)
    {
        PublishText("系统", $"关闭光感上报异常: {ex.Message}");
    }

    _lightReportEnabled = false;
}

async Task ExitTestModeIfNeeded()
{
    if (!_shellConnected || !_enteredTestMode)
    {
        return;
    }

    try
    {
        string output = await SendShellCommand("exit-test", BleCommandTimeoutMs);
        PublishText("BLE输出", output);
        PublishValue("EXIT_TEST_MODE", "", output.Contains("[OK]") || output.Contains("exit"));
    }
    catch (Exception ex)
    {
        PublishText("系统", $"退出产测模式异常: {ex.Message}");
    }

    _enteredTestMode = false;
}

async Task<string> SendShellCommand(string command, int timeoutMs)
{
    if (_shellHandle == null || !_shellConnected)
    {
        throw new InvalidOperationException("Shell会话未建立");
    }

    ProcessAsyncFunctions.ProcessAsync.ClearOutputBuffer(_shellHandle);
    ProcessAsyncFunctions.ProcessAsync.ClearErrorBuffer(_shellHandle);
    PublishText("Shell命令", $"V3> {command}");

    return await ProcessAsyncFunctions.ProcessAsync.SendCommandAndReadAsync(
        _shellHandle,
        command,
        "V3>",
        timeoutMs,
        GetExecutionToken());
}

bool ConnectLightSource(string portName, int baudRate, out string message)
{
    _lightSourceHandle = new SerialPortFunction.SerialPortClient();
    bool opened = _lightSourceHandle.Open(portName, baudRate, System.IO.Ports.Parity.None, 8, System.IO.Ports.StopBits.One);
    _lightSourceConnected = opened && _lightSourceHandle.IsOpen;
    message = _lightSourceConnected
        ? $"光源控制器连接成功: {portName}@{baudRate}"
        : $"光源控制器连接失败: {portName}@{baudRate}";
    return _lightSourceConnected;
}

bool SetLightSourceSingleChannel(int channel, int current, out string message)
{
    if (!_lightSourceConnected || _lightSourceHandle == null)
    {
        message = "光源控制器未连接";
        return false;
    }

    if (channel < 1 || channel > 4)
    {
        message = $"光源通道无效: {channel}";
        return false;
    }

    if (current < 0) current = 0;
    if (current > 255) current = 255;

    string cmd = BuildLightSourceSingleChannelCommand(channel, current);
    _lightSourceHandle.ClearBuffers();
    _lightSourceHandle.WriteHex(cmd);
    message = $"设置光源 CH{channel} 电流 = {current}, cmd={cmd}";
    return true;
}

string BuildLightSourceSingleChannelCommand(int channel, int current)
{
    byte startFrame = 0x24;
    byte channelByte = (byte)channel;
    byte currentByte = (byte)current;
    byte checksum = (byte)(startFrame ^ channelByte ^ currentByte);
    return $"{startFrame:X2} {channelByte:X2} {currentByte:X2} {checksum:X2}";
}

void TurnOffLightSourceIfNeeded()
{
    if (!LightSourceTurnOffAtEnd || !_lightSourceConnected)
    {
        return;
    }

    try
    {
        string message;
        SetLightSourceSingleChannel(LightSourceChannel, LightSourceOffCurrent, out message);
        PublishText("光源", $"结束置零: {message}");
    }
    catch (Exception ex)
    {
        PublishText("光源", $"结束置零异常: {ex.Message}");
    }
}

void DisconnectLightSource()
{
    try
    {
        if (_lightSourceHandle != null)
        {
            _lightSourceHandle.Close();
        }
    }
    catch { }

    _lightSourceHandle = null;
    _lightSourceConnected = false;
}

void CloseShell()
{
    if (_shellHandle == null)
    {
        _shellConnected = false;
        _enteredTestMode = false;
        _lightReportEnabled = false;
        return;
    }

    if (_shellConnected)
    {
        try
        {
            ProcessAsyncFunctions.ProcessAsync.SendCommand(_shellHandle, "quit");
            System.Threading.Thread.Sleep(300);
        }
        catch (Exception ex)
        {
            PublishText("系统", $"发送 quit 关闭 BLE Shell 异常: {ex.Message}");
        }
    }

    try { ProcessAsyncFunctions.ProcessAsync.ClearOutputBuffer(_shellHandle); } catch { }
    try { ProcessAsyncFunctions.ProcessAsync.ClearErrorBuffer(_shellHandle); } catch { }
    try { ProcessAsyncFunctions.ProcessAsync.Close(_shellHandle); } catch { }

    PublishText("系统", "BLE Shell 会话已关闭");
    _shellHandle = null;
    _shellConnected = false;
    _enteredTestMode = false;
    _lightReportEnabled = false;
}

bool TryParseMacFromPcbaSn(string pcbaSn, out string macAddress)
{
    macAddress = "";

    if (string.IsNullOrEmpty(pcbaSn))
    {
        PublishText("解析错误", "PCBA SN 为空");
        return false;
    }

    if (pcbaSn.Length < MacStartIndex + MacLength)
    {
        PublishText("解析错误", $"PCBA SN 长度不足: {pcbaSn.Length}位，需要至少{MacStartIndex + MacLength}位");
        return false;
    }

    string macHex = pcbaSn.Substring(MacStartIndex, MacLength).ToUpper();
    if (macHex.Length != 12 || macHex.Any(c => !((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))))
    {
        PublishText("解析错误", $"MAC 地址格式无效: {macHex}");
        return false;
    }

    macAddress = string.Join(":", Enumerable.Range(0, 6).Select(i => macHex.Substring(i * 2, 2)));
    PublishText("MAC解析", $"PCBA SN: {pcbaSn}");
    PublishText("MAC解析", $"提取位置: [{MacStartIndex}:{MacStartIndex + MacLength}] = {macHex}");
    PublishText("MAC解析", $"格式化MAC: {macAddress}");
    return true;
}

void ResetState()
{
    sfcKeyList.Clear();
    _mesFinalSummary = "";
    _pcbaSn = "";
    _deviceMac = "";
    _currentLightCurrent = 0;
    _shellConnected = false;
    _enteredTestMode = false;
    _lightReportEnabled = false;
    _summary.Clear();
    _dutValues.Clear();
}

void MesStart()
{
    if (!MESEnble)
    {
        PublishValue("MES启动", "跳过MES", true);
        return;
    }

    PublishText("MES", "━━━━━ MES 启动 ━━━━━");
    string failmessage = "";
    bool ok = _mes.MesStart(SerialNumber, out failmessage);
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

void MESgetsfckeybysfc()
{
    if (!MESEnble)
    {
        PublishValue("主板条码", _pcbaSn, !string.IsNullOrWhiteSpace(_pcbaSn));
        return;
    }

    sfcKeyList.Clear();
    string failmessage = "";
    bool result = _mes.MES_getsfckey_bysfc(SerialNumber, ref failmessage, ref sfcKeyList);
    PublishText("MES", $"桩号绑定结果：{result}, 消息：{failmessage}");

    string targetKey = MESBoardKeyName;
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

void MesComplete()
{
    PublishText("MES", "━━━━━ MES 结束上报 ━━━━━");
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
        bool ok = _mes.MesComplete(SerialNumber, "", out failmessage);
        _mesFinalSummary = ok ? "MES完成上报成功" : $"MES完成上报失败: {failmessage}";
        PublishText("MES", ok ? "MesComplete(PASS) 成功" : $"MesComplete 失败: {failmessage}");
        PublishState("MES完成上报", ok);
        return;
    }

    string mesFailReason = GetMesFailReason();
    PublishText("MES", $"MesNC 失败项目: {mesFailReason}");
    bool failOk = _mes.MesNCComplete(SerialNumber, mesFailReason, out failmessage);
    _mesFinalSummary = failOk ? "MESNC上报成功" : $"MESNC上报失败: {failmessage}";
    PublishText("MES", failOk ? "MesNCComplete(FAIL) 成功" : $"MesNCComplete 失败: {failmessage}");
    PublishState("MESNC上报", failOk);
}

protected override Task OnTestFinishedAsync()
{
    PublishText("系统", "测试生命周期结束，执行清理...");
    MesComplete();
    UIFocusByRole("txtSerialNumber");
    UIClearByRole("txtSerialNumber");
    PublishText("系统", string.IsNullOrWhiteSpace(_mesFinalSummary) ? "清理完成" : $"清理完成 | {_mesFinalSummary}");
    return Task.CompletedTask;
}

async Task SleepAsync(int milliseconds)
{
    int remainingMs = Math.Max(0, milliseconds);
    while (remainingMs > 0)
    {
        if (IsStopRequested())
        {
            return;
        }

        int chunkMs = Math.Min(remainingMs, 100);
        await Task.Delay(chunkMs, GetExecutionToken());
        remainingMs -= chunkMs;
    }
}
