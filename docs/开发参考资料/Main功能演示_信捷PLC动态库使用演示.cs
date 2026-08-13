
XJPLC PLC = new XJPLC();
string currentComPort = "COM11"; // 默认串口
int currentBaudRate = 115200;     // 默认波特率
byte currentSlaveId = 1;        // 默认从站地址

/// <summary>
/// 主测试入口
/// </summary>
void MainTest()
{
    TestManage.UpdateStatus("=== 信捷PLC完整功能演示开始 ===");
    
    // 1. 基础连接测试
    BasicConnectionTest();
    
    // 2. 全地址类型测试
    ComprehensiveAddressTest();
    
    // 3. 数据类型转换测试
    DataConversionTest();
    
    // 4. 批量操作性能测试
    BatchOperationTest();
    
    // 5. 错误处理演示
    ErrorHandlingDemo();
    
    TestManage.UpdateStatus("=== 信捷PLC完整功能演示结束 ===");
}

void OnTestFinished()
{
    TestManage.UpdateStatus("信捷PLC完整测试完成！！！");
}

#region 1. 基础连接测试
/// <summary>
/// 基础连接和通信测试
/// </summary>
void BasicConnectionTest()
{
    TestManage.UpdateStatus("\n【1. 基础连接测试】");
    
    // 串口RTU连接测试
    TestManage.UpdateStatus($"尝试连接串口: {currentComPort}, 波特率: {currentBaudRate}, 从站地址: {currentSlaveId}");
    
    if (PLC.Open(currentComPort, currentBaudRate, 8, "N", 1, currentSlaveId))
    {
        TestManage.UpdateStatus($"✓ 信捷PLC RTU连接成功");
        
        try
        {
            // 基础读写测试
            TestManage.UpdateStatus("执行基础读写测试...");
            
            // 测试M线圈
            PLC.WriteSingleCoil("M100", true);
            TestManage.SleepMilliseconds(100);
            bool[] mStatus = PLC.ReadCoils("M100", 1);
            TestManage.UpdateStatus($"M100写入true，读取: {mStatus[0]}");
            
            // 测试D寄存器
            PLC.WriteSingleRegister("D100", 1234);
            TestManage.SleepMilliseconds(100);
            ushort[] dValue = PLC.ReadHoldingRegisters("D100", 1);
            TestManage.UpdateStatus($"D100写入1234，读取: {dValue[0]}");
            
            // 清理
            PLC.WriteSingleCoil("M100", false);
            PLC.WriteSingleRegister("D100", 0);
            
            TestManage.UpdateStatus("✓ 基础读写测试完成");
        }
        catch (Exception ex)
        {
            TestManage.UpdateStatus($"✗ 基础读写测试失败: {ex.Message}");
            
            // 检查通信错误
            if (PLC.GetCommunicationError())
            {
                TestManage.UpdateStatus($"通信错误码: 0x{PLC.GetLastErrorCode():X2}");
                PLC.ClearCommunicationError();
            }
        }
        finally
        {
            PLC.Close();
        }
    }
    else
    {
        TestManage.UpdateStatus($"✗ 信捷PLC串口连接失败，请检查串口参数和连接");
        TestManage.UpdateStatus("请确认：");
        TestManage.UpdateStatus($"  - 串口 {currentComPort} 是否正确");
        TestManage.UpdateStatus($"  - 波特率 {currentBaudRate} 是否匹配");
        TestManage.UpdateStatus($"  - 从站地址 {currentSlaveId} 是否正确");
        TestManage.UpdateStatus("  - PLC是否已连接并上电");
    }
}
#endregion

#region 2. 全地址类型测试
/// <summary>
/// 全面的地址类型测试
/// </summary>
void ComprehensiveAddressTest()
{
    TestManage.UpdateStatus("\n【2. 全地址类型测试】");
    
    if (PLC.Open(currentComPort, currentBaudRate, 8, "N", 1, currentSlaveId))
    {
        try
        {
            // M继电器测试
            TestMCoils();
            
            // D寄存器测试
            TestDRegisters();
            
            // X输入测试
            TestXInputs();
            
            // Y输出测试
            TestYOutputs();
            
            // S状态继电器测试
            TestSCoils();
            
            // T定时器测试
            TestTimers();
            
            // C计数器测试
            TestCounters();
        }
        catch (Exception ex)
        {
            TestManage.UpdateStatus($"地址测试异常: {ex.Message}");
        }
        finally
        {
            PLC.Close();
        }
    }
}

void TestMCoils()
{
    TestManage.UpdateStatus("\nM内部继电器测试:");
    try
    {
        // 单个M继电器测试
        PLC.WriteSingleCoil("M200", true);
        TestManage.SleepMilliseconds(50);
        bool[] m200 = PLC.ReadCoils("M200", 1);
        TestManage.UpdateStatus($"M200 单个操作: {m200[0]}");
        
        // 批量M继电器测试
        bool[] batchMValues = { true, false, true, true, false };
        PLC.WriteMultipleCoils("M1100", batchMValues);
        TestManage.SleepMilliseconds(50);
        bool[] readMValues = PLC.ReadCoils("M1100", 5);
        
        TestManage.UpdateStatus("M1100-M1104 批量操作:");
        for (int i = 0; i < readMValues.Length; i++)
        {
            TestManage.UpdateStatus($"  M{300 + i}: {readMValues[i]}");
        }
        
        // 清理
        PLC.WriteMultipleCoils("M200", new bool[10]);
        PLC.WriteMultipleCoils("M300", new bool[10]);
    }
    catch (Exception ex)
    {
        TestManage.UpdateStatus($"M继电器测试失败: {ex.Message}");
    }
}

void TestDRegisters()
{
    TestManage.UpdateStatus("\nD数据寄存器测试:");
    try
    {
        // 单个D寄存器测试
        PLC.WriteSingleRegister("D500", 12345);
        TestManage.SleepMilliseconds(50);
        ushort[] d500 = PLC.ReadHoldingRegisters("D500", 1);
        TestManage.UpdateStatus($"D500 单个操作: {d500[0]}");
        
        // 批量D寄存器测试
        ushort[] batchDValues = { 100, 200, 300, 400, 500 };
        PLC.WriteMultipleRegisters("D600", batchDValues);
        TestManage.SleepMilliseconds(50);
        ushort[] readDValues = PLC.ReadHoldingRegisters("D600", 5);
        
        TestManage.UpdateStatus("D600-D604 批量操作:");
        for (int i = 0; i < readDValues.Length; i++)
        {
            TestManage.UpdateStatus($"  D{600 + i}: {readDValues[i]}");
        }
        
        // 清理
        PLC.WriteMultipleRegisters("D500", new ushort[10]);
        PLC.WriteMultipleRegisters("D600", new ushort[10]);
    }
    catch (Exception ex)
    {
        TestManage.UpdateStatus($"D寄存器测试失败: {ex.Message}");
    }
}

void TestXInputs()
{
    TestManage.UpdateStatus("\nX输入继电器测试:");
    try
    {
        // 读取X输入（八进制地址）
        bool[] xInputs = PLC.ReadDiscreteInputs("X0", 8);  // X0~X7
        TestManage.UpdateStatus("X输入状态:");
        for (int i = 0; i < xInputs.Length; i++)
        {
            TestManage.UpdateStatus($"  X{i}: {xInputs[i]}");
        }
    }
    catch (Exception ex)
    {
        TestManage.UpdateStatus($"X输入测试失败: {ex.Message}");
    }
}

void TestYOutputs()
{
    TestManage.UpdateStatus("\nY输出继电器测试:");
    try
    {
        // 控制Y输出（八进制地址）
        TestManage.UpdateStatus("测试Y输出控制:");
        bool[] yOutputs = { true, false, true, false };
        PLC.WriteMultipleCoils("Y0", yOutputs);
        TestManage.SleepMilliseconds(500);
        
        bool[] readYOutputs = PLC.ReadCoils("Y0", 4);
        for (int i = 0; i < readYOutputs.Length; i++)
        {
            TestManage.UpdateStatus($"  Y{i}: {readYOutputs[i]}");
        }
        
        // 清理Y输出
        PLC.WriteMultipleCoils("Y0", new bool[4]);
    }
    catch (Exception ex)
    {
        TestManage.UpdateStatus($"Y输出测试失败: {ex.Message}");
    }
}

void TestSCoils()
{
    TestManage.UpdateStatus("\nS状态继电器测试:");
    try
    {
        // S状态继电器读写测试
        bool[] sValues = { true, false, true };
        PLC.WriteMultipleCoils("S10", sValues);
        TestManage.SleepMilliseconds(50);
        bool[] readSValues = PLC.ReadCoils("S10", 3);
        
        TestManage.UpdateStatus("S10-S12 操作:");
        for (int i = 0; i < readSValues.Length; i++)
        {
            TestManage.UpdateStatus($"  S{10 + i}: {readSValues[i]}");
        }
        
        // 清理
        PLC.WriteMultipleCoils("S10", new bool[3]);
    }
    catch (Exception ex)
    {
        TestManage.UpdateStatus($"S状态继电器测试失败: {ex.Message}");
    }
}

void TestTimers()
{
    TestManage.UpdateStatus("\n定时器测试:");
    try
    {
        // 读取定时器状态
        bool[] timerCoils = PLC.ReadCoils("T0", 3);
        TestManage.UpdateStatus("定时器线圈状态:");
        for (int i = 0; i < timerCoils.Length; i++)
        {
            TestManage.UpdateStatus($"  T{i}线圈: {timerCoils[i]}");
        }
        
        // 读取定时器当前值
        ushort[] timerValues = PLC.ReadHoldingRegisters("T0", 3);
        TestManage.UpdateStatus("定时器当前值:");
        for (int i = 0; i < timerValues.Length; i++)
        {
            TestManage.UpdateStatus($"  T{i}值: {timerValues[i]}");
        }
    }
    catch (Exception ex)
    {
        TestManage.UpdateStatus($"定时器测试失败: {ex.Message}");
    }
}

void TestCounters()
{
    TestManage.UpdateStatus("\n计数器测试:");
    try
    {
        // 计数器线圈状态
        bool[] counterCoils = PLC.ReadCoils("C0", 3);
        TestManage.UpdateStatus("计数器线圈状态:");
        for (int i = 0; i < counterCoils.Length; i++)
        {
            TestManage.UpdateStatus($"  C{i}线圈: {counterCoils[i]}");
        }
        
        // 计数器当前值
        ushort[] counterValues = PLC.ReadHoldingRegisters("C0", 3);
        TestManage.UpdateStatus("计数器当前值:");
        for (int i = 0; i < counterValues.Length; i++)
        {
            TestManage.UpdateStatus($"  C{i}值: {counterValues[i]}");
        }
    }
    catch (Exception ex)
    {
        TestManage.UpdateStatus($"计数器测试失败: {ex.Message}");
    }
}
#endregion

#region 3. 数据类型转换测试
/// <summary>
/// 数据类型转换功能测试
/// </summary>
void DataConversionTest()
{
    TestManage.UpdateStatus("\n【3. 数据类型转换测试】");
    
    if (PLC.Open(currentComPort, currentBaudRate, 8, "N", 1, currentSlaveId))
    {
        try
        {
            TestManage.UpdateStatus("整数转换测试:");
            
            // 32位整数转换测试
            int originalInt32 = 123456789;
            ushort[] int32Data = XJPLCDataConverter.FromInt32(originalInt32);
            PLC.WriteMultipleRegisters("D1000", int32Data);
            TestManage.SleepMilliseconds(50);
            
            ushort[] readInt32Data = PLC.ReadHoldingRegisters("D1000", 2);
            int convertedInt32 = XJPLCDataConverter.ToInt32(readInt32Data);
            TestManage.UpdateStatus($"Int32: 原始={originalInt32}, 转换后={convertedInt32}, 匹配={originalInt32 == convertedInt32}");
            
            // 浮点数转换测试
            TestManage.UpdateStatus("\n浮点数转换测试:");
            float originalFloat = 3.14159f;
            ushort[] floatData = XJPLCDataConverter.FromFloat(originalFloat);
            PLC.WriteMultipleRegisters("D1010", floatData);
            TestManage.SleepMilliseconds(50);
            
            ushort[] readFloatData = PLC.ReadHoldingRegisters("D1010", 2);
            float convertedFloat = XJPLCDataConverter.ToFloat(readFloatData);
            TestManage.UpdateStatus($"Float: 原始={originalFloat:F5}, 转换后={convertedFloat:F5}, 差值={Math.Abs(originalFloat - convertedFloat):E3}");
            
            // 16位整数转换测试
            TestManage.UpdateStatus("\n16位整数转换测试:");
            short originalInt16 = -12345;
            PLC.WriteSingleRegister("D1020", (ushort)originalInt16);
            TestManage.SleepMilliseconds(50);
            
            ushort[] readInt16Data = PLC.ReadHoldingRegisters("D1020", 1);
            short convertedInt16 = XJPLCDataConverter.ToInt16(readInt16Data);
            TestManage.UpdateStatus($"Int16: 原始={originalInt16}, 转换后={convertedInt16}, 匹配={originalInt16 == convertedInt16}");
            
            // 字符串转换测试
            TestManage.UpdateStatus("\n字符串转换测试:");
            string originalString = "XINJE PLC";
            ushort[] stringData = XJPLCDataConverter.FromString(originalString);
            PLC.WriteMultipleRegisters("D1030", stringData);
            TestManage.SleepMilliseconds(50);
            
            //ushort[] readStringData = PLC.ReadHoldingRegisters("D1030", stringData.Length);
            //string convertedString = XJPLCDataConverter.ToString(readStringData);
           // TestManage.UpdateStatus($"String: 原始='{originalString}', 转换后='{convertedString}', 匹配={originalString == convertedString}");
            
            // 清理
            PLC.WriteMultipleRegisters("D1000", new ushort[40]);
        }
        catch (Exception ex)
        {
            TestManage.UpdateStatus($"数据转换测试失败: {ex.Message}");
        }
        finally
        {
            PLC.Close();
        }
    }
}
#endregion

#region 4. 批量操作性能测试
/// <summary>
/// 批量操作性能测试
/// </summary>
void BatchOperationTest()
{
    TestManage.UpdateStatus("\n【4. 批量操作性能测试】");
    
    if (PLC.Open(currentComPort, currentBaudRate, 8, "N", 1, currentSlaveId))
    {
        try
        {
            // 大批量线圈操作
            TestManage.UpdateStatus("大批量线圈操作测试:");
            DateTime startTime = DateTime.Now;
            
            bool[] largeBoolArray = new bool[100];
            for (int i = 0; i < largeBoolArray.Length; i++)
            {
                largeBoolArray[i] = (i % 2 == 0); // 交替true/false
            }
            
            PLC.WriteMultipleCoils("M1000", largeBoolArray);
            TestManage.SleepMilliseconds(100);
            bool[] readLargeBoolArray = PLC.ReadCoils("M1000", 100);
            
            DateTime endTime = DateTime.Now;
            double elapsedMs = (endTime - startTime).TotalMilliseconds;
            
            // 验证数据正确性
            bool dataCorrect = true;
            for (int i = 0; i < largeBoolArray.Length; i++)
            {
                if (largeBoolArray[i] != readLargeBoolArray[i])
                {
                    dataCorrect = false;
                    break;
                }
            }
            
            TestManage.UpdateStatus($"100个线圈批量操作: 耗时{elapsedMs:F1}ms, 数据正确={dataCorrect}");
            
            // 大批量寄存器操作
            TestManage.UpdateStatus("大批量寄存器操作测试:");
            startTime = DateTime.Now;
            
            ushort[] largeRegArray = new ushort[50];
            for (int i = 0; i < largeRegArray.Length; i++)
            {
                largeRegArray[i] = (ushort)(i * 100);
            }
            
            PLC.WriteMultipleRegisters("D2000", largeRegArray);
            TestManage.SleepMilliseconds(100);
            ushort[] readLargeRegArray = PLC.ReadHoldingRegisters("D2000", 50);
            
            endTime = DateTime.Now;
            elapsedMs = (endTime - startTime).TotalMilliseconds;
            
            // 验证数据正确性
            dataCorrect = true;
            for (int i = 0; i < largeRegArray.Length; i++)
            {
                if (largeRegArray[i] != readLargeRegArray[i])
                {
                    dataCorrect = false;
                    break;
                }
            }
            
            TestManage.UpdateStatus($"50个寄存器批量操作: 耗时{elapsedMs:F1}ms, 数据正确={dataCorrect}");
            
            // 清理
            PLC.WriteMultipleCoils("M1000", new bool[100]);
            PLC.WriteMultipleRegisters("D2000", new ushort[50]);
        }
        catch (Exception ex)
        {
            TestManage.UpdateStatus($"批量操作测试失败: {ex.Message}");
        }
        finally
        {
            PLC.Close();
        }
    }
}
#endregion

#region 5. 错误处理演示
/// <summary>
/// 错误处理演示
/// </summary>
void ErrorHandlingDemo()
{
    TestManage.UpdateStatus("\n【5. 错误处理演示】");
    
    if (PLC.Open(currentComPort, currentBaudRate, 8, "N", 1, currentSlaveId))
    {
        try
        {
            // 测试无效地址
            TestManage.UpdateStatus("测试无效地址处理:");
            try
            {
                bool[] invalidResult = PLC.ReadCoils("M99999", 1);
                TestManage.UpdateStatus("✗ 无效地址M99999未被拒绝");
            }
            catch (Exception ex)
            {
                TestManage.UpdateStatus($"✓ 无效地址M99999被正确拒绝: {ex.Message}");
            }
            
            // 测试超出数量限制
            TestManage.UpdateStatus("测试数量限制处理:");
            try
            {
                bool[] tooManyResult = PLC.ReadCoils("M0", 10000);
                TestManage.UpdateStatus("✗ 过大数量请求未被限制");
            }
            catch (Exception ex)
            {
                TestManage.UpdateStatus($"✓ 过大数量请求被正确限制: {ex.Message}");
            }
            
            // 测试不支持的地址类型
            TestManage.UpdateStatus("测试不支持地址类型:");
            try
            {
                bool[] unsupportedResult = PLC.ReadCoils("Z0", 1);
                TestManage.UpdateStatus("✗ 不支持的地址类型Z未被拒绝");
            }
            catch (Exception ex)
            {
                TestManage.UpdateStatus($"✓ 不支持的地址类型Z被正确拒绝: {ex.Message}");
            }
            
            // 演示通信错误恢复
            TestManage.UpdateStatus("通信错误状态演示:");
            if (PLC.GetCommunicationError())
            {
                TestManage.UpdateStatus($"当前通信错误状态: true, 错误码: 0x{PLC.GetLastErrorCode():X2}");
                PLC.ClearCommunicationError();
                TestManage.UpdateStatus("通信错误状态已清除");
            }
            else
            {
                TestManage.UpdateStatus("当前通信状态正常");
            }
        }
        catch (Exception ex)
        {
            TestManage.UpdateStatus($"错误处理演示异常: {ex.Message}");
        }
        finally
        {
            PLC.Close();
        }
    }
}
#endregion


