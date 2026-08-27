 
object PLCHandle=null;

void MainTest()
{  
    Initilize();
    Test();
}

void OnTestFinished()
{ 
    TestManage.UpdateStatus("Test finished8888888888.");
    SerialPort.Close(PLCHandle);
}



void Initilize()
{
    TestManage.UpdateStatus("************** 读取配置信息 **************");

    PLCHandle= SerialPort.Open("COM8", 9600,8,"N",1,false); 
    bool IsOpen1= SerialPort.IsConnected(PLCHandle);   
   
    if(IsOpen1)
    {
        TestManage.UpdateStatus("串口连接成功！！");           
    }
    else
    {
        if (!IsOpen1)
        {         
            TestManage.UpdateStatus("PLC 串口连接失败!");
        }     
                                          
    }               
}

void Test()
{
    TestManage.SetTestGroupName(""); 

    string cmdstr= SetSingleChannel(1,22);
    TestManage.UpdateStatus($"单通道指令: {cmdstr}");
    string Result=SerialPort.SendHexString(PLCHandle,cmdstr,"",100);

    TestManage.UpdateStatus($"单通道回复: {Result}");

    cmdstr= SetAllChannels(30,50,50,50);
    TestManage.UpdateStatus($"多通道指令: {cmdstr}");
    //Result=SerialPort.SendHexString(PLCHandle,cmdstr,"",100);
    //TestManage.UpdateStatus($"四通道回复: {Result}");

    
}


/// <summary>
/// 单通道设置命令
/// </summary>
/// <param name="channel">通道号 (十进制，如1)</param>
/// <param name="brightness">亮度值 (十进制，如42)</param>
/// <returns>十六进制指令字符串</returns>
string SetSingleChannel(int channel, int brightness)
{
    byte startFrame = 0x24;
    byte channelByte = (byte)channel;  // 十进制转字节
    byte brightnessByte = (byte)brightness;  // 十进制转字节
    
    // 计算校验 - 前3字节异或和
    byte checksum = (byte)(startFrame ^ channelByte ^ brightnessByte);
    
    return $"{startFrame:X2} {channelByte:X2} {brightnessByte:X2} {checksum:X2}";
}

/// <summary>
/// 四通道同时设置命令
/// </summary>
/// <param name="channel1">通道1亮度 (十进制，如30)</param>
/// <param name="channel2">通道2亮度 (十进制，如30)</param>
/// <param name="channel3">通道3亮度 (十进制，如30)</param>
/// <param name="channel4">通道4亮度 (十进制，如30)</param>
/// <returns>十六进制指令字符串</returns>
string SetAllChannels(int channel1, int channel2, int channel3, int channel4)
{
    byte startFrame = 0x25;
    byte channel1Byte = (byte)channel1;  // 十进制转字节
    byte channel2Byte = (byte)channel2;  // 十进制转字节
    byte channel3Byte = (byte)channel3;  // 十进制转字节
    byte channel4Byte = (byte)channel4;  // 十进制转字节
    
    // 计算校验 - 前5字节异或和
    byte checksum = (byte)(startFrame ^ channel1Byte ^ channel2Byte ^ channel3Byte ^ channel4Byte);
    
    return $"{startFrame:X2} {channel1Byte:X2} {channel2Byte:X2} {channel3Byte:X2} {channel4Byte:X2} {checksum:X2}";
}