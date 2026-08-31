// ============================================
// V3 Bluetooth PER Standalone Validation
//
// Purpose:
// 1. Keep only BLE shell connection, optional factory-test preparation, DUT HCI RX, and CMW100 GPRF ARB packet source.
// 2. Validate BLE RX PER on 2402 / 2440 / 2480 MHz before merging into the full production script.
// 3. Do not run MES, SN writing, triple application, display test, complete, or API report.
// ============================================

CancellationToken GetExecutionToken() => Context?.CancellationToken ?? CancellationToken.None;
bool IsStopRequested() => GetExecutionToken().IsCancellationRequested;

CancellationTokenSource _testCts;

ProcessAsync _processAsync = new ProcessAsync();
ProcessAsyncFunctions.IProcessHandle _shellHandle = null;
bool _shellConnected = false;
bool _enteredTestMode = false;

object _cmwVisaHandle = null;
System.IO.Ports.SerialPort _blePerSerialPort = null;
List<(string Label, double Per, bool Pass)> _blePerFinalResults = new List<(string Label, double Per, bool Pass)>();

string _pcbaSn = "";
string _deviceMac = "";

const string StepBleConnect = "STEP_BLE_CONNECT";
const string StepPrepare = "STEP_PREPARE";
const string StepCmwConnect = "STEP_CMW_CONNECT";
const string StepUartConnect = "STEP_HCI_UART_CONNECT";
const string StepPerTest = "STEP_BLE_PER";

// ============================================
// Parameters
// ============================================
string V3ExePath => Param<string>("V3_ExePath", @"C:\bat\v3_exe.bat");
int BleConnectMode => Param<int>("BLE_ConnectMode", 3); // 1=barcode MAC, 2=parameter MAC, 3=auto strongest
string BleDeviceMac => Param<string>("BLE_DeviceMac", "");
string BlePort => Param<string>("BLE_Port", "");
int MacStartIndex => Param<int>("MacStartIndex", 4);
int MacLength => Param<int>("MacLength", 12);
int BleConnectTimeoutMs => Param<int>("BLE_ConnectTimeoutMs", 15000);
int BleCommandTimeoutMs => Param<int>("BLE_CommandTimeoutMs", 10000);
bool BleEnterTestMode => Param<bool>("BLE_EnterTestMode", true);
string BlePrepareShellCommand => Param<string>("BlePer_PrepareShellCommand", "");
string BlePerNonSignalingShellCommand => Param<string>("BlePer_NonSignalingShellCommand", string.IsNullOrWhiteSpace(BlePrepareShellCommand) ? "bt-nonsig-on" : BlePrepareShellCommand);
bool BlePerRequireNonSignalingShellCommand => Param<bool>("BlePer_RequireNonSignalingShellCommand", true);
int BlePrepareDelayMs => Param<int>("BlePer_PrepareDelayMs", 300);
bool BleCloseShellBeforeHci => Param<bool>("BlePer_CloseShellBeforeHci", true);
int BleDisconnectSettleMs => Param<int>("BlePer_DisconnectSettleMs", 500);

string BlePerScenarioList => Param<string>("BlePer_ScenarioList", "2402:1M,2440:1M,2480:1M");
int BlePerTxCount => Param<int>("BlePer_TxCount", 1000);
double BlePerMaxPercent => Param<double>("BlePer_MaxPercent", 30.8);
bool BlePerContinueOnFail => Param<bool>("BlePer_ContinueOnFail", true);
int BlePerMaxAttempts => Param<int>("BlePer_MaxAttempts", Param<int>("BlePer_RetestCount", 3));
int BlePerRetestDelayMs => Param<int>("BlePer_RetestDelayMs", 300);

string BlePerUartPort => Param<string>("BlePer_UartPort", "COM11");
int BlePerUartBaudRate => Param<int>("BlePer_UartBaudRate", 115200);
int BlePerUartDataBits => Param<int>("BlePer_UartDataBits", 8);
string BlePerUartParity => Param<string>("BlePer_UartParity", "None");
string BlePerUartStopBits => Param<string>("BlePer_UartStopBits", "One");
int BlePerUartReadTimeoutMs => Param<int>("BlePer_UartReadTimeoutMs", 3000);
int BlePerUartQuietMs => Param<int>("BlePer_UartQuietMs", 120);
string BlePerUartInitCommandsHex => Param<string>("BlePer_UartInitCommandsHex", "");
string BlePerUartExitCommandsHex => Param<string>("BlePer_UartExitCommandsHex", "");
string BlePerHciResetHex => Param<string>("BlePer_HciResetHex", "01030C00");
string BlePerHciTestEndHex => Param<string>("BlePer_HciTestEndHex", "011F2000");
bool BlePerVerifyHciResponse => Param<bool>("BlePer_VerifyHciResponse", true);

string BlePerCmwVisaAddress => Param<string>("BlePer_CmwVisaAddress", "TCPIP0::DESKTOP-TC6GTTI::inst0::INSTR");
int BlePerCmwCommandDelayMs => Param<int>("BlePer_CmwCommandDelayMs", 120);
string BlePerCmwWaveformFile => Param<string>("BlePer_CmwWaveformFile", @"@WAVEFORM\BT_LE_TestPacket_P9_1M.wv");
string BlePerCmwArbRepetition => Param<string>("BlePer_CmwArbRepetition", "SINGle");
int BlePerCmwArbCycles => Param<int>("BlePer_CmwArbCycles", BlePerTxCount);
int BlePerCmwArbCompleteCycles => Param<int>("BlePer_CmwArbCompleteCycles", Math.Max(0, BlePerCmwArbCycles - 1));
int BlePerCmwArbPollIntervalMs => Param<int>("BlePer_CmwArbPollIntervalMs", 200);
int BlePerCmwArbTimeoutMs => Param<int>("BlePer_CmwArbTimeoutMs", 10000);
int BlePerCmwTriggerWaitMs => Param<int>("BlePer_CmwTriggerWaitMs", 1000);
bool BlePerCmwWaitArbScount => Param<bool>("BlePer_CmwWaitArbScount", false);
bool BlePerCmwUseGuiRfConfig => Param<bool>("BlePer_CmwUseGuiRfConfig", true);
bool BlePerCmwEnableFixedInit => Param<bool>("BlePer_CmwEnableFixedInit", false);
bool BlePerCmwSetRfLevel => Param<bool>("BlePer_CmwSetRfLevel", false);
double BlePerCmwTxPowerDbm => Param<double>("BlePer_CmwTxPowerDbm", -50.0);
bool BlePerCmwCheckErrorAfterScenario => Param<bool>("BlePer_CmwCheckErrorAfterScenario", true);

// ============================================
// Entry
// ============================================
[UIAction("test.start")]
[UIAction("input.serialnumber")]
async Task MainTest(string serialnumber)
{
    Context.SerialNumber = serialnumber?.Trim() ?? "";
    _pcbaSn = Context.SerialNumber;

    BeginRunUi(_pcbaSn);
    ResetStepIndicators();

    try
    {
        ShowConfiguration();

        SetCurrentStep("步骤1/BLE连接", "连接 DUT BLE Shell，用于进入产测或发送准备命令");
        await ConnectBleShell();
        PublishState(StepBleConnect, true, "完成", "未完成");
        if (IsStopRequested()) return;

        if (BleEnterTestMode)
        {
            SetCurrentStep("步骤2/进入产测", "发送 enter 进入产测模式");
            await EnterTestMode();
            if (IsStopRequested()) return;
        }

        SetCurrentStep("步骤3/进入非信令", "发送 BLE PER 非信令模式命令");
        await EnterBlePerNonSignalingMode();
        PublishState(StepPrepare, true, "完成", "未完成");

        if (BleCloseShellBeforeHci && _shellConnected)
        {
            PublishText("BLE", "关闭 BLE Shell，释放链路，准备切换 HCI UART");
            CloseShell();
            await DelayMs(Math.Max(0, BleDisconnectSettleMs));
        }

        SetCurrentStep("步骤4/连接CMW100", "打开 CMW100 VISA 连接并初始化 GPRF ARB");
        InitializeBlePerCmw();
        InitializeBlePerCmwGprfArb();
        PublishState(StepCmwConnect, true, "完成", "未完成");

        SetCurrentStep("步骤5/打开HCI UART", "打开 DUT HCI/产测串口");
        OpenBlePerSerialPort();
        await SendBlePerUartCommandHexList(BlePerUartInitCommandsHex, "UART初始化");
        PublishState(StepUartConnect, true, "完成", "未完成");

        SetCurrentStep("步骤6/三频点PER", "CMW100 发包，DUT HCI 读取收包数");
        bool allPassed = await RunBlePerAllScenarios();
        PublishState(StepPerTest, allPassed, "通过", "失败");

        await SendBlePerUartCommandHexList(BlePerUartExitCommandsHex, "UART退出");

        PublishProgress("测试进度", 100, allPassed ? "通过" : "失败");
        PublishStatus("状态", allPassed ? "测试完成" : "测试失败");

        if (allPassed)
        {
            PassTest("BLE PER专项验证");
        }
        else
        {
            FailTest("BLE PER专项验证", "存在频点 PER 超限或测试异常");
        }
    }
    catch (OperationCanceledException)
    {
        PublishText("系统", "测试已取消");
        PublishStatus("状态", "已取消");
    }
    catch (Exception ex)
    {
        PublishText("错误", $"BLE PER专项验证异常: {ex.Message}");
        PublishAlarm("BLE PER专项验证失败", "error");
        PublishStatus("状态", "测试失败");
        FailTest("BLE PER专项验证", ex.Message);
    }
    finally
    {
        await SafeCleanupAsync();
    }
}

[UIAction("test.stop")]
void StopTest()
{
    PublishText("系统", "停止 BLE PER 专项验证");
    _testCts?.Cancel();
    PublishStatus("状态", "停止中");
    TryStopBlePerCmwGprfArb();
    CloseBlePerResources();
    CloseShell();
}

// ============================================
// UI / configuration
// ============================================
void BeginRunUi(string serialnumber)
{
    _testCts?.Cancel();
    _testCts = new CancellationTokenSource();
    Context.CancellationToken = _testCts.Token;

    PublishStatus("状态", "运行中");
    PublishText("测试状态", "━━━━━ V3 蓝牙 PER 专项验证 ━━━━━");
    PublishText("测试状态", $"输入条码/MAC: {serialnumber}");
    PublishValue("测试输入", serialnumber, true);
    PublishProgress("测试进度", 1, "已启动");
}

void ResetStepIndicators()
{
    PublishState(StepBleConnect, false, "完成", "未完成");
    PublishState(StepPrepare, false, "完成", "未完成");
    PublishState(StepCmwConnect, false, "完成", "未完成");
    PublishState(StepUartConnect, false, "完成", "未完成");
    PublishState(StepPerTest, false, "通过", "未完成");
}

void SetCurrentStep(string stepName, string instruction)
{
    PublishStatus("当前步骤", stepName);
    PublishText("作业指导", instruction);
}

void ShowConfiguration()
{
    PublishText("配置", "━━━━━ BLE PER 专项参数 ━━━━━");
    PublishText("配置", $"BLE连接模式 = {BleConnectMode} ({GetBleConnectModeName()})");
    PublishText("配置", $"BLE参数MAC = {BleDeviceMac}");
    PublishText("配置", $"BLE端口 = {(string.IsNullOrWhiteSpace(BlePort) ? "默认/自动" : BlePort)}");
    PublishText("配置", $"进入产测模式 = {BleEnterTestMode}");
    PublishText("配置", $"非信令命令 = {(string.IsNullOrWhiteSpace(BlePerNonSignalingShellCommand) ? "<none>" : BlePerNonSignalingShellCommand)}");
    PublishText("配置", $"非信令命令必发 = {BlePerRequireNonSignalingShellCommand}");
    PublishText("配置", $"HCI串口 = {BlePerUartPort} @ {BlePerUartBaudRate}");
    PublishText("配置", $"CMW VISA = {BlePerCmwVisaAddress}");
    PublishText("配置", $"CMW固定初始化 = {(BlePerCmwEnableFixedInit ? "开启" : "屏蔽")}");
    PublishText("配置", $"CMW波形 = {(string.IsNullOrWhiteSpace(BlePerCmwWaveformFile) ? "沿用仪器当前波形" : BlePerCmwWaveformFile)}");
    PublishText("配置", $"三频点 = {BlePerScenarioList}");
    PublishText("配置", $"CMW ARB cycles = {BlePerCmwArbCycles}, PER TxCount = {BlePerTxCount}");
    PublishText("配置", $"PER上限 = {BlePerMaxPercent:F3}%");
    PublishText("配置", $"PER最多测试次数 = {BlePerMaxAttempts}");
}

// ============================================
// BLE shell
// ============================================
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

string BuildBleConnectArguments()
{
    _deviceMac = "";
    string portArguments = BuildBlePortArguments();

    if (BleConnectMode == 1)
    {
        if (!TryParseMacFromPcbaSn(_pcbaSn, out _deviceMac))
        {
            throw new InvalidOperationException("连接模式1要求从输入条码解析MAC，但当前输入格式无效");
        }

        PublishText("MAC解析", $"从输入解析 MAC: {_deviceMac}");
        return JoinArguments(portArguments, $"--mac {_deviceMac}");
    }

    if (BleConnectMode == 2)
    {
        string macSource = !string.IsNullOrWhiteSpace(BleDeviceMac) ? BleDeviceMac : _pcbaSn;
        if (!TryNormalizeMac(macSource, out _deviceMac))
        {
            throw new InvalidOperationException("连接模式2要求 BLE_DeviceMac 或输入内容为有效MAC");
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

    return $"--port {QuoteArgument(port)}";
}

string JoinArguments(params string[] parts)
{
    return string.Join(" ", parts.Where(p => !string.IsNullOrWhiteSpace(p)));
}

string QuoteArgument(string value)
{
    if (string.IsNullOrEmpty(value))
    {
        return "\"\"";
    }

    if (value.Any(char.IsWhiteSpace) || value.Contains("\""))
    {
        return $"\"{value.Replace("\"", "\\\"")}\"";
    }

    return value;
}

async Task ConnectBleShell()
{
    string arguments = BuildBleConnectArguments();

    PublishText("BLE", "━━━ BLE连接 ━━━");
    PublishText("BLE", $"连接模式: {BleConnectMode} ({GetBleConnectModeName()})");
    PublishText("BLE", $"目标MAC: {(string.IsNullOrWhiteSpace(_deviceMac) ? "自动最强信号" : _deviceMac)}");
    PublishText("BLE", $"启动参数: {(string.IsNullOrWhiteSpace(arguments) ? "<empty>" : arguments)}");
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
        await DelayMs(200);
        return;
    }

    PublishValue("BLE_CONNECT", "", false);
    throw new Exception($"BLE连接失败: {output}");
}

async Task EnterTestMode()
{
    string output = await SendShellCommand("enter", BleCommandTimeoutMs);
    PublishText("BLE输出", output);
    bool ok = output.Contains("[OK]") || output.Contains("产测模式") || output.Contains("factory");
    PublishValue("ENTER_TEST_MODE", "", ok);
    if (!ok)
    {
        throw new Exception($"进入产测模式失败: {output}");
    }

    _enteredTestMode = true;
    await DelayMs(200);
}

async Task EnterBlePerNonSignalingMode()
{
    if (string.IsNullOrWhiteSpace(BlePerNonSignalingShellCommand))
    {
        if (BlePerRequireNonSignalingShellCommand)
        {
            throw new Exception("BlePer_NonSignalingShellCommand 未配置，禁止关闭 BLE Shell 后直接进入 HCI RX 测试");
        }

        PublishText("BLE PER", "未配置非信令命令，按参数允许跳过");
        return;
    }

    PublishText("BLE PER", $"进入非信令命令: {BlePerNonSignalingShellCommand}");
    string output = await SendShellCommand(BlePerNonSignalingShellCommand, BleCommandTimeoutMs);
    PublishText("BLE输出", output);
    string normalizedOutput = output.Replace("\r", "").Replace("\n", "").Trim();
    bool isKnownNonSignalingReady = normalizedOutput.Contains("[FAIL]", StringComparison.OrdinalIgnoreCase)
        && normalizedOutput.Contains("None_", StringComparison.OrdinalIgnoreCase);
    bool ok = output.Contains("[OK]", StringComparison.OrdinalIgnoreCase) || isKnownNonSignalingReady;
    if (!ok)
    {
        throw new Exception($"进入 BLE 非信令模式失败: {output}");
    }

    if (isKnownNonSignalingReady)
    {
        PublishText("BLE PER", "bt-nonsig-on 返回 [FAIL] None_，按现场定义判定为已进入非信令模式");
    }

    await DelayMs(Math.Max(0, BlePrepareDelayMs));
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
            ProcessAsync.SendCommand(_shellHandle, "quit");
            System.Threading.Thread.Sleep(300);
        }

        ProcessAsync.ClearOutputBuffer(_shellHandle);
        ProcessAsync.ClearErrorBuffer(_shellHandle);
        ProcessAsync.Close(_shellHandle);
        PublishText("系统", "BLE Shell 会话已关闭");
    }
    catch (Exception ex)
    {
        PublishText("Warning", $"关闭 BLE Shell 异常: {ex.Message}");
    }
    finally
    {
        _shellHandle = null;
        _shellConnected = false;
        _enteredTestMode = false;
    }
}

bool TryNormalizeMac(string input, out string macAddress)
{
    macAddress = "";
    if (string.IsNullOrWhiteSpace(input))
    {
        return false;
    }

    string hex = input.Replace(":", "").Replace("-", "").Trim().ToUpperInvariant();
    if (!System.Text.RegularExpressions.Regex.IsMatch(hex, "^[0-9A-F]{12}$"))
    {
        return false;
    }

    macAddress = string.Join(":", Enumerable.Range(0, 6).Select(i => hex.Substring(i * 2, 2)));
    return true;
}

bool TryParseMacFromPcbaSn(string sn, out string macAddress)
{
    macAddress = "";
    if (string.IsNullOrWhiteSpace(sn) || sn.Length < MacStartIndex + MacLength)
    {
        return false;
    }

    string macHex = sn.Substring(MacStartIndex, MacLength);
    return TryNormalizeMac(macHex, out macAddress);
}

// ============================================
// BLE PER flow
// ============================================
async Task<bool> RunBlePerAllScenarios()
{
    var scenarios = ParseBlePerScenarios();
    if (scenarios.Count == 0)
    {
        throw new Exception("BLE PER 场景为空，请检查 BlePer_ScenarioList");
    }

    bool allPassed = true;
    _blePerFinalResults.Clear();
    PublishProgress("测试进度", 65, "BLE PER三频点");

    foreach (var scenario in scenarios)
    {
        if (IsStopRequested())
        {
            throw new OperationCanceledException();
        }

        bool passed = await RunBlePerScenarioWithRetest(scenario);
        allPassed &= passed;

        if (!passed && !BlePerContinueOnFail)
        {
            break;
        }
    }

    PublishBlePerFinalResults();
    return allPassed;
}

async Task<bool> RunBlePerScenarioWithRetest((string Label, int FrequencyMHz, int ChannelIndex, int PhyCode, string PhyName) scenario)
{
    int maxAttempts = Math.Max(1, BlePerMaxAttempts);
    Exception lastError = null;
    double? lastPer = null;

    for (int attempt = 1; attempt <= maxAttempts; attempt++)
    {
        try
        {
            PublishText("BLE PER", $"{scenario.Label} Attempt {attempt}/{maxAttempts}");
            bool isFinalAttempt = attempt >= maxAttempts;
            var result = await RunBlePerScenarioAttempt(scenario, attempt, isFinalAttempt);
            lastPer = result.Per;
            if (result.Pass)
            {
                _blePerFinalResults.Add((scenario.Label, result.Per, true));
                return true;
            }

            if (attempt < maxAttempts)
            {
                PublishText("BLE PER", $"{scenario.Label} Attempt {attempt} PER未达标，准备复测");
            }
        }
        catch (Exception ex)
        {
            lastError = ex;
            if (IsBlePerCmwNonRetriableError(ex))
            {
                PublishText("BLE PER错误", $"{scenario.Label} CMW100发包/仪器异常，停止本频点复测: {ex.Message}");
                _blePerFinalResults.Add((scenario.Label, lastPer ?? 100.0, false));
                return false;
            }

            if (attempt < maxAttempts)
            {
                PublishText("BLE PER", $"{scenario.Label} Attempt {attempt} 异常，准备复测");
            }
        }

        if (attempt < maxAttempts)
        {
            await DelayMs(Math.Max(0, BlePerRetestDelayMs));
        }
    }

    if (lastError != null)
    {
        PublishText("BLE PER错误", $"{scenario.Label} 总共测试{maxAttempts}次后仍异常: {lastError.Message}");
    }
    else
    {
        PublishText("BLE PER", $"{scenario.Label} 总共测试{maxAttempts}次后仍 PER 超限");
    }

    _blePerFinalResults.Add((scenario.Label, lastPer ?? 100.0, false));
    return false;
}

bool IsBlePerCmwNonRetriableError(Exception ex)
{
    for (Exception current = ex; current != null; current = current.InnerException)
    {
        string message = current.Message ?? "";
        if (message.Contains("CMW100", StringComparison.OrdinalIgnoreCase) ||
            message.Contains("SCOunt", StringComparison.OrdinalIgnoreCase) ||
            message.Contains("VISA", StringComparison.OrdinalIgnoreCase) ||
            message.Contains("VI_ERROR", StringComparison.OrdinalIgnoreCase))
        {
            return true;
        }
    }

    return false;
}

void PublishBlePerFinalResults()
{
    foreach (var result in _blePerFinalResults)
    {
        PublishValue($"BLE_PER_{result.Label}", Math.Round(result.Per, 3), "%", 0, BlePerMaxPercent, result.Pass);
    }
}

async Task<(bool Pass, double Per)> RunBlePerScenarioAttempt((string Label, int FrequencyMHz, int ChannelIndex, int PhyCode, string PhyName) scenario, int attempt, bool isFinalAttempt)
{
    PublishText("BLE PER", $"━━━ 场景 {scenario.Label} ━━━");

    bool rxStarted = false;
    byte[] testEndResponse = null;
    Exception pendingError = null;
    int cmwCycles = 0;

    try
    {
        var resetResponse = await SendBlePerUartCommandHex(BlePerHciResetHex, "HCI Reset");
        EnsureBlePerResponse(resetResponse, "030C00", $"{scenario.Label} Reset应答异常");

        string startRxHex = BuildBlePerRxStartHex(scenario.ChannelIndex, scenario.PhyCode);
        var startRxResponse = await SendBlePerUartCommandHex(startRxHex, $"Start RX {scenario.Label}");
        EnsureBlePerResponse(startRxResponse, "332000", $"{scenario.Label} Start RX应答异常");
        rxStarted = true;

        cmwCycles = await RunBlePerCmwGprfArbScenario(scenario);
    }
    catch (Exception ex)
    {
        pendingError = ex;
    }
    finally
    {
        TryStopBlePerCmwGprfArb();

        if (rxStarted)
        {
            try
            {
                testEndResponse = await SendBlePerUartCommandHex(BlePerHciTestEndHex, $"Test End {scenario.Label} Attempt {attempt}");
                EnsureBlePerResponse(testEndResponse, "1F2000", $"{scenario.Label} Test End应答异常");
            }
            catch (Exception ex)
            {
                pendingError = new Exception($"{scenario.Label} Start RX succeeded but Test End failed; cannot read RxCount/PER, current attempt failed: {ex.Message}", pendingError ?? ex);
            }
        }
    }

    if (pendingError != null)
    {
        throw pendingError;
    }

    int rxCount = ParseBlePerRxCount(testEndResponse);
    double per = BlePerTxCount <= 0 ? 100.0 : ((double)(BlePerTxCount - rxCount) / BlePerTxCount) * 100.0;
    bool pass = BlePerTxCount > 0 && rxCount >= 0 && per <= BlePerMaxPercent;

    PublishText("BLE PER", $"{scenario.Label} -> RxCount={rxCount}, TxCount={BlePerTxCount}, CmwCycles={cmwCycles}, PER={per:F3}%");

    if (!pass && isFinalAttempt)
    {
        PublishText("BLE PER", $"{scenario.Label} PER超限: {per:F3}% > {BlePerMaxPercent:F3}%");
    }

    return (pass, per);
}

List<(string Label, int FrequencyMHz, int ChannelIndex, int PhyCode, string PhyName)> ParseBlePerScenarios()
{
    var list = new List<(string Label, int FrequencyMHz, int ChannelIndex, int PhyCode, string PhyName)>();
    string raw = string.IsNullOrWhiteSpace(BlePerScenarioList) ? "2402:1M,2440:1M,2480:1M" : BlePerScenarioList;
    var parts = raw.Split(new[] { ',', ';', '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);

    foreach (string part in parts)
    {
        string token = part.Trim();
        if (string.IsNullOrWhiteSpace(token))
        {
            continue;
        }

        var fields = token.Split(':', StringSplitOptions.RemoveEmptyEntries);
        string freqToken = fields[0].Trim().ToUpperInvariant();
        string phyToken = fields.Length > 1 ? fields[1].Trim().ToUpperInvariant() : "1M";

        int freqMHz = freqToken switch
        {
            "LOW" => 2402,
            "MID" => 2440,
            "HIGH" => 2480,
            _ => int.Parse(freqToken)
        };

        int channelIndex = FrequencyToBlePerChannel(freqMHz);
        int phyCode = phyToken switch
        {
            "1M" => 0x01,
            "2M" => 0x02,
            _ => throw new Exception($"不支持的 PHY: {phyToken}")
        };

        list.Add(($"{freqMHz}_{phyToken}", freqMHz, channelIndex, phyCode, phyToken));
    }

    return list;
}

int FrequencyToBlePerChannel(int freqMHz)
{
    return freqMHz switch
    {
        2402 => 0,
        2440 => 19,
        2480 => 39,
        _ when freqMHz >= 2402 && freqMHz <= 2480 => freqMHz - 2402,
        _ => throw new Exception($"不支持的 BLE 频点: {freqMHz} MHz")
    };
}

string BuildBlePerRxStartHex(int channelIndex, int phyCode)
{
    byte[] data = new byte[]
    {
        0x01, 0x33, 0x20, 0x03,
        (byte)channelIndex,
        (byte)phyCode,
        0x00
    };
    return BytesToHex(data);
}

// ============================================
// CMW100 GPRF ARB
// ============================================
void InitializeBlePerCmw()
{
    try
    {
        if (_cmwVisaHandle != null)
        {
            try
            {
                string idn = VISA.Read(_cmwVisaHandle, "*IDN?\n");
                PublishText("CMW100", $"复用已有连接: {idn?.Trim()}");
                PublishState("CMW100连接", true);
                return;
            }
            catch
            {
                _cmwVisaHandle = null;
            }
        }

        _cmwVisaHandle = VISA.Open(BlePerCmwVisaAddress);
        if (_cmwVisaHandle == null)
        {
            throw new Exception("VISA.Open 返回空句柄");
        }

        string idnNow = VISA.Read(_cmwVisaHandle, "*IDN?\n");
        PublishText("CMW100", $"仪表信息: {idnNow?.Trim()}");
        PublishState("CMW100连接", true);
    }
    catch (Exception ex)
    {
        PublishState("CMW100连接", false);
        throw new Exception($"CMW100 初始化失败: {ex.Message}");
    }
}

void InitializeBlePerCmwGprfArb()
{
    PublishText("CMW100", "初始化 GPRF ARB 发包源");
    CmwWrite("*CLS");

    if (!BlePerCmwEnableFixedInit)
    {
        PublishText("CMW100", "CMW100固定初始化已屏蔽：不发送 BBMode/Cycles/Level/STATe ON/AutoStart/波形加载，只保留后续切频和手动触发");
        string skipInitError = CmwWriteRead("SYSTem:ERRor?");
        EnsureCmwNoError(skipInitError, "CMW100 GPRF ARB 初始化");
        return;
    }

    PublishText("CMW100", "按脚本初始化 CMW100 GPRF ARB：波形、功率、输出状态固定初始化；后续场景只切频点并触发发包");
    CmwWrite("SOURce:GPRF:GEN:BBMode ARB");
    CmwWrite($"SOURce:GPRF:GEN:ARB:CYCLes {Math.Max(1, BlePerCmwArbCycles)}");
    CmwWrite($"SOURce:GPRF:GEN:ARB:REPetition {BlePerCmwArbRepetition}");
    CmwWrite($"SOURce:GPRF:GEN:RFSettings:LEVel {BlePerCmwTxPowerDbm.ToString("F1", System.Globalization.CultureInfo.InvariantCulture)}");
    string stateResponse = CmwWriteRead("SOURce:GPRF:GEN:STATe ON;*OPC?");
    PublishText("CMW100", $"GPRF 输出打开 OPC={stateResponse?.Trim()}");
    CmwWrite("TRIGger:GPRF:GEN:ARB:RETRigger ON");
    CmwWrite("TRIGger:GPRF:GEN:ARB:AUTostart ON");

    if (!string.IsNullOrWhiteSpace(BlePerCmwWaveformFile))
    {
        string waveformFile = BlePerCmwWaveformFile.Replace("\"", "\"\"");
        CmwWrite($"SOURce:GPRF:GEN:ARB:FILE \"{waveformFile}\"");
        string waveform = CmwWriteRead("SOURce:GPRF:GEN:ARB:FILE?");
        PublishText("CMW100波形", waveform?.Trim() ?? "");
    }
    else
    {
        PublishText("CMW100波形", "未配置波形文件，沿用仪器当前已选择波形");
    }

    string error = CmwWriteRead("SYSTem:ERRor?");
    EnsureCmwNoError(error, "CMW100 GPRF ARB 初始化");
}

async Task<int> RunBlePerCmwGprfArbScenario((string Label, int FrequencyMHz, int ChannelIndex, int PhyCode, string PhyName) scenario)
{
    PublishText("CMW100", $"GPRF ARB 发包: {scenario.Label}, {scenario.FrequencyMHz} MHz, cycles={BlePerCmwArbCycles}");

    CmwWrite("*CLS");
    CmwWrite($"SOURce:GPRF:GEN:RFSettings:FREQuency {scenario.FrequencyMHz}MHz");
    if (!BlePerCmwUseGuiRfConfig)
    {
        CmwWrite($"SOURce:GPRF:GEN:ARB:REPetition {BlePerCmwArbRepetition}");
        CmwWrite($"SOURce:GPRF:GEN:ARB:CYCLes {Math.Max(1, BlePerCmwArbCycles)}");
    }
    CmwWrite("TRIGger:GPRF:GEN:ARB:MANual:EXECute");

    int cycles = 0;
    if (BlePerCmwWaitArbScount)
    {
        cycles = await WaitBlePerCmwArbComplete(scenario);
    }
    else
    {
        PublishText("CMW100", $"{scenario.Label}: 已触发 ARB，固定等待 {BlePerCmwTriggerWaitMs} ms 后读取 DUT 收包数");
        await DelayMs(Math.Max(0, BlePerCmwTriggerWaitMs));
    }

    if (BlePerCmwCheckErrorAfterScenario)
    {
        string error = CmwWriteRead("SYSTem:ERRor?");
        EnsureCmwNoError(error, $"CMW100 {scenario.Label}");
    }

    return cycles;
}

async Task<int> WaitBlePerCmwArbComplete((string Label, int FrequencyMHz, int ChannelIndex, int PhyCode, string PhyName) scenario)
{
    DateTime deadline = DateTime.Now.AddMilliseconds(Math.Max(500, BlePerCmwArbTimeoutMs));
    int targetCycles = BlePerCmwArbCompleteCycles > 0
        ? BlePerCmwArbCompleteCycles
        : Math.Max(0, BlePerCmwArbCycles - 1);

    int lastCycles = 0;
    string lastResponse = "";

    while (DateTime.Now < deadline)
    {
        lastResponse = CmwWriteRead("SOURce:GPRF:GEN:ARB:SCOunt?");
        if (TryParseCmwArbScount(lastResponse, out double countTime, out int cycles, out int samplesCurrent))
        {
            lastCycles = cycles;
            PublishText("CMW100发包进度", $"{scenario.Label}: time={countTime:F3}s, cycles={cycles}, samples={samplesCurrent}");

            if (targetCycles <= 0 || cycles >= targetCycles)
            {
                return cycles;
            }
        }
        else
        {
            PublishText("CMW100发包进度", $"{scenario.Label}: 无法解析 SCOunt 返回: {lastResponse?.Trim()}");
        }

        await DelayMs(Math.Max(50, BlePerCmwArbPollIntervalMs));
    }

    throw new Exception($"{scenario.Label} CMW100 ARB 发包超时，最后返回: {lastResponse?.Trim()}, cycles={lastCycles}");
}

bool TryParseCmwArbScount(string response, out double countTime, out int cycles, out int samplesCurrent)
{
    countTime = 0;
    cycles = 0;
    samplesCurrent = 0;

    if (string.IsNullOrWhiteSpace(response))
    {
        return false;
    }

    string clean = response.Trim().Trim('"');
    var parts = clean.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
    if (parts.Length < 3)
    {
        return false;
    }

    bool okTime = double.TryParse(parts[0].Trim(), System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out countTime);
    bool okCycles = int.TryParse(parts[1].Trim(), System.Globalization.NumberStyles.Integer, System.Globalization.CultureInfo.InvariantCulture, out cycles);
    bool okSamples = int.TryParse(parts[2].Trim(), System.Globalization.NumberStyles.Integer, System.Globalization.CultureInfo.InvariantCulture, out samplesCurrent);

    return okTime && okCycles && okSamples;
}

void TryStopBlePerCmwGprfArb()
{
    PublishText("CMW100", "保持 CMW APP/GUI 当前输出状态，脚本不发送 GPRF STATe OFF");
}

void CmwWrite(string command)
{
    if (_cmwVisaHandle == null)
    {
        throw new Exception("CMW100 未连接");
    }

    PublishText("CMW100命令", command);
    VISA.Write(_cmwVisaHandle, command + "\n");
    System.Threading.Thread.Sleep(Math.Max(0, BlePerCmwCommandDelayMs));
}

string CmwWriteRead(string command)
{
    if (_cmwVisaHandle == null)
    {
        throw new Exception("CMW100 未连接");
    }

    PublishText("CMW100命令", command);
    string response = VISA.Read(_cmwVisaHandle, command + "\n");
    PublishText("CMW100响应", response?.Trim() ?? "");
    System.Threading.Thread.Sleep(Math.Max(0, BlePerCmwCommandDelayMs));
    return response ?? "";
}

void EnsureCmwNoError(string response, string stageName)
{
    string text = response?.Trim() ?? "";
    if (!text.StartsWith("0,", StringComparison.Ordinal))
    {
        throw new Exception($"{stageName} 仪器报错: {text}");
    }
}

string EscapeCmwString(string value)
{
    return (value ?? "").Replace("'", "''");
}

// ============================================
// DUT HCI UART
// ============================================
void OpenBlePerSerialPort()
{
    try
    {
        if (_blePerSerialPort != null && _blePerSerialPort.IsOpen)
        {
            PublishState("BLE_PER_UART连接", true);
            return;
        }

        _blePerSerialPort = new System.IO.Ports.SerialPort(
            BlePerUartPort,
            BlePerUartBaudRate,
            ParseBlePerParity(BlePerUartParity),
            BlePerUartDataBits,
            ParseBlePerStopBits(BlePerUartStopBits));

        _blePerSerialPort.ReadTimeout = BlePerUartReadTimeoutMs;
        _blePerSerialPort.WriteTimeout = BlePerUartReadTimeoutMs;
        _blePerSerialPort.Open();
        _blePerSerialPort.DiscardInBuffer();
        _blePerSerialPort.DiscardOutBuffer();

        PublishText("BLE_PER_UART", $"串口已打开: {BlePerUartPort} @ {BlePerUartBaudRate}");
        PublishState("BLE_PER_UART连接", true);
    }
    catch (Exception ex)
    {
        PublishState("BLE_PER_UART连接", false);
        throw new Exception($"BLE PER 串口打开失败: {ex.Message}");
    }
}

System.IO.Ports.Parity ParseBlePerParity(string parity)
{
    return Enum.TryParse<System.IO.Ports.Parity>(parity, true, out var parsed)
        ? parsed
        : System.IO.Ports.Parity.None;
}

System.IO.Ports.StopBits ParseBlePerStopBits(string stopBits)
{
    return Enum.TryParse<System.IO.Ports.StopBits>(stopBits, true, out var parsed)
        ? parsed
        : System.IO.Ports.StopBits.One;
}

async Task SendBlePerUartCommandHexList(string commandsHex, string stageName)
{
    if (string.IsNullOrWhiteSpace(commandsHex))
    {
        return;
    }

    var parts = commandsHex.Split(new[] { ',', ';', '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);
    foreach (var part in parts)
    {
        await SendBlePerUartCommandHex(part.Trim(), stageName);
    }
}

async Task<byte[]> SendBlePerUartCommandHex(string hex, string stageName)
{
    if (_blePerSerialPort == null || !_blePerSerialPort.IsOpen)
    {
        throw new Exception("BLE PER 串口未打开");
    }

    byte[] command = HexToBytes(hex);
    _blePerSerialPort.DiscardInBuffer();
    _blePerSerialPort.DiscardOutBuffer();

    PublishText("BLE_PER_UART_TX", $"{stageName}: {BytesToHex(command)}");
    _blePerSerialPort.Write(command, 0, command.Length);
    _blePerSerialPort.BaseStream.Flush();

    var response = await ReadBlePerSerialResponse();
    PublishText("BLE_PER_UART_RX", $"{stageName}: {BytesToHex(response)}");
    return response;
}

async Task<byte[]> ReadBlePerSerialResponse()
{
    var buffer = new List<byte>();
    DateTime deadline = DateTime.Now.AddMilliseconds(Math.Max(200, BlePerUartReadTimeoutMs));
    DateTime lastDataAt = DateTime.MinValue;

    while (DateTime.Now < deadline)
    {
        int available = _blePerSerialPort?.BytesToRead ?? 0;
        if (available > 0)
        {
            byte[] temp = new byte[available];
            int read = _blePerSerialPort.Read(temp, 0, temp.Length);
            if (read > 0)
            {
                buffer.AddRange(temp.Take(read));
                lastDataAt = DateTime.Now;
            }
        }
        else
        {
            if (buffer.Count > 0 && lastDataAt != DateTime.MinValue &&
                (DateTime.Now - lastDataAt).TotalMilliseconds >= Math.Max(20, BlePerUartQuietMs))
            {
                break;
            }

            await DelayMs(20);
        }
    }

    return buffer.ToArray();
}

void EnsureBlePerResponse(byte[] response, string expectedHexFragment, string errorMessage)
{
    if (!BlePerVerifyHciResponse)
    {
        return;
    }

    string actual = BytesToHex(response);
    if (response == null || response.Length == 0 || !actual.Contains(expectedHexFragment, StringComparison.OrdinalIgnoreCase))
    {
        throw new Exception($"{errorMessage}: {actual}");
    }
}

int ParseBlePerRxCount(byte[] response)
{
    if (response == null || response.Length < 2)
    {
        throw new Exception("Test End 返回长度不足，无法解析 RxCount");
    }

    int low = response[response.Length - 2];
    int high = response[response.Length - 1];
    return low | (high << 8);
}

byte[] HexToBytes(string hex)
{
    string clean = new string((hex ?? "").Where(IsHexChar).ToArray());
    if (clean.Length == 0 || clean.Length % 2 != 0)
    {
        throw new Exception($"非法HEX字符串: {hex}");
    }

    byte[] data = new byte[clean.Length / 2];
    for (int i = 0; i < data.Length; i++)
    {
        data[i] = Convert.ToByte(clean.Substring(i * 2, 2), 16);
    }
    return data;
}

bool IsHexChar(char c)
{
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

string BytesToHex(byte[] bytes)
{
    if (bytes == null || bytes.Length == 0)
    {
        return "";
    }

    return BitConverter.ToString(bytes).Replace("-", "");
}

// ============================================
// Cleanup / timer
// ============================================
async Task SafeCleanupAsync()
{
    try
    {
        if (_shellConnected && _enteredTestMode && !BleCloseShellBeforeHci)
        {
            string output = await SendShellCommand("exit-test", BleCommandTimeoutMs);
            PublishText("BLE输出", output);
        }
    }
    catch (Exception ex)
    {
        PublishText("Warning", $"退出产测模式异常: {ex.Message}");
    }

    TryStopBlePerCmwGprfArb();
    CloseBlePerResources();
    CloseShell();
}

void CloseBlePerResources()
{
    try
    {
        if (_blePerSerialPort != null)
        {
            if (_blePerSerialPort.IsOpen)
            {
                _blePerSerialPort.Close();
            }

            _blePerSerialPort.Dispose();
            _blePerSerialPort = null;
            PublishText("系统", "BLE PER 串口已关闭");
        }
    }
    catch (Exception ex)
    {
        PublishText("Warning", $"关闭 BLE PER 串口异常: {ex.Message}");
    }

    try
    {
        if (_cmwVisaHandle != null)
        {
            VISA.Close(_cmwVisaHandle);
            _cmwVisaHandle = null;
            PublishText("系统", "CMW100 连接已关闭");
        }
    }
    catch (Exception ex)
    {
        PublishText("Warning", $"关闭 CMW100 连接异常: {ex.Message}");
    }
}

async Task DelayMs(int ms)
{
    if (ms <= 0)
    {
        return;
    }

    await Task.Delay(ms, GetExecutionToken());
}
