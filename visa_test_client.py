import socket
import time

class VisaTestClient:
    def __init__(self, host='localhost', port=5025):
        self.host = host
        self.port = port
        self.socket = None

    def connect(self):
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((self.host, self.port))
            print(f"Connected to VISA device at {self.host}:{self.port}")
            return True
        except Exception as e:
            print(f"Connection failed: {e}")
            return False

    def send_command(self, command):
        if not self.socket:
            print("Not connected")
            return None
        
        try:
            self.socket.sendall((command + '\n').encode('utf-8'))
            response = self.socket.recv(1024).decode('utf-8').strip()
            return response
        except Exception as e:
            print(f"Send failed: {e}")
            return None

    def close(self):
        if self.socket:
            self.socket.close()
            print("Disconnected")

def test_visa_device():
    client = VisaTestClient(port=5026)
    
    if not client.connect():
        return
    
    print("\n=== Testing VISA Device Simulator ===")
    
    # 测试设备标识
    print("\n1. Query device ID:")
    response = client.send_command("*IDN?")
    print(f"   Response: {response}")
    
    # 测试设置电压
    print("\n2. Set voltage to 12.0V:")
    response = client.send_command("VOLT 12.0")
    print(f"   Response: {response}")
    
    # 测试设置电流
    print("\n3. Set current to 2.5A:")
    response = client.send_command("CURR 2.5")
    print(f"   Response: {response}")
    
    # 测试打开输出
    print("\n4. Turn output ON:")
    response = client.send_command("OUTP ON")
    print(f"   Response: {response}")
    
    # 测试读取电压
    print("\n5. Measure voltage:")
    response = client.send_command("MEAS:VOLT:DC?")
    print(f"   Response: {response} V")
    
    # 测试读取电流
    print("\n6. Measure current:")
    response = client.send_command("MEAS:CURR:DC?")
    print(f"   Response: {response} A")
    
    # 测试读取电流（带参数）
    print("\n7. Measure current with range:")
    response = client.send_command("MEAS:CURR:DC? 500e-3")
    print(f"   Response: {response} A")
    
    # 测试关闭输出
    print("\n8. Turn output OFF:")
    response = client.send_command("OUTP OFF")
    print(f"   Response: {response}")
    
    # 测试读取关闭后的电压电流
    print("\n9. Measure after OFF:")
    voltage = client.send_command("MEAS:VOLT:DC?")
    current = client.send_command("MEAS:CURR:DC?")
    print(f"   Voltage: {voltage} V")
    print(f"   Current: {current} A")
    
    # 测试错误查询
    print("\n10. Query error status:")
    response = client.send_command("SYST:ERR?")
    print(f"   Response: {response}")
    
    # 测试复位
    print("\n11. Reset device:")
    response = client.send_command("RST")
    print(f"   Response: {response}")
    
    client.close()
    print("\n=== Test completed ===")

if __name__ == '__main__':
    test_visa_device()