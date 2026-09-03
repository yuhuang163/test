# -*- coding: utf-8 -*-
"""
信捷 PLC (Modbus RTU/TCP) & 海康扫码枪 组合模拟器
功能特性：
1. 模拟信捷 PLC：
   - 支持 Modbus RTU（串口，可选通过 pyserial 监听指定 COM 口，默认波特率 115200/19200）
   - 支持 Modbus TCP（端口 502，支持标准 0x01 读线圈、0x02 读离散输入）
   - 支持 0x01 读 M 线圈（如 M100）、0x02 读 X 输入（如 X0）、0x05/0x0F 写线圈、0x03/0x06 读写 D 寄存器
   - 按回车键触发按键信号（置 1），上位机轮询读取后自动弹起复位（置 0）
2. 模拟海康扫码枪：
   - TCP Server 监听 2001 端口
   - 收到上位机触发字符后，延时模拟拍照并返回条码字符串
"""

import socket
import threading
import time
import argparse

# 全局按键线圈状态
PLC_TRIGGER = False
PLC_SLAVE_ID = 1

# 简单存储线圈与寄存器状态
M_COILS = {}       # M线圈 (0~7999)
X_INPUTS = {}      # X离散输入 (0x4000+)
D_REGISTERS = {}   # D保持寄存器 (0~7999)


def crc16_modbus(data: bytes) -> bytes:
    """计算 Modbus RTU CRC16 校验码（低字节在前）"""
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return bytes([crc & 0xFF, (crc >> 8) & 0xFF])


def process_modbus_pdu(func_code: int, pdu_body: bytes) -> bytes:
    """
    处理 Modbus PDU 业务逻辑（RTU 与 TCP 核心通用）
    """
    global PLC_TRIGGER

    # 0x01: 读线圈 (Read Coils - M线圈)
    # 0x02: 读离散输入 (Read Discrete Inputs - X输入)
    if func_code in (0x01, 0x02):
        if len(pdu_body) >= 4:
            start_addr = (pdu_body[0] << 8) | pdu_body[1]
            quantity = (pdu_body[2] << 8) | pdu_body[3]
        else:
            start_addr = 100
            quantity = 1

        # 若处于按键触发态，返回 ON (1)，并自动弹起复位为 OFF (0)
        if PLC_TRIGGER:
            state_val = 0x01
            PLC_TRIGGER = False
            addr_desc = f"X{start_addr - 0x4000:o}" if start_addr >= 0x4000 else f"M{start_addr}"
            print(f"\n>> [信捷PLC] 检测到上位机读取按键状态({addr_desc})，已下发触发信号 (ON=1) 并自动复位！\n")
        else:
            state_val = 0x00

        byte_count = (quantity + 7) // 8
        resp_data = bytes([state_val] + [0x00] * (byte_count - 1))
        return bytes([func_code, byte_count]) + resp_data

    # 0x05: 写单个线圈 (Write Single Coil)
    elif func_code == 0x05:
        if len(pdu_body) >= 4:
            addr = (pdu_body[0] << 8) | pdu_body[1]
            val = (pdu_body[2] << 8) | pdu_body[3]
            M_COILS[addr] = (val == 0xFF00)
            print(f">> [信捷PLC] 收到写线圈指令: M{addr} = {M_COILS[addr]}")
            return bytes([func_code]) + pdu_body[:4]
        return bytes([func_code | 0x80, 0x03])

    # 0x0F: 写多个线圈 (Write Multiple Coils)
    elif func_code == 0x0F:
        if len(pdu_body) >= 4:
            return bytes([func_code]) + pdu_body[:4]
        return bytes([func_code | 0x80, 0x03])

    # 0x03: 读保持寄存器 (Read Holding Registers - D寄存器)
    elif func_code == 0x03:
        if len(pdu_body) >= 4:
            start_addr = (pdu_body[0] << 8) | pdu_body[1]
            quantity = (pdu_body[2] << 8) | pdu_body[3]
            byte_count = quantity * 2
            resp_bytes = bytearray()
            for i in range(quantity):
                val = D_REGISTERS.get(start_addr + i, 1234)
                resp_bytes.extend([(val >> 8) & 0xFF, val & 0xFF])
            return bytes([func_code, byte_count]) + bytes(resp_bytes)
        return bytes([func_code | 0x80, 0x03])

    # 0x06: 写单个寄存器 (Write Single Register)
    elif func_code == 0x06:
        if len(pdu_body) >= 4:
            addr = (pdu_body[0] << 8) | pdu_body[1]
            val = (pdu_body[2] << 8) | pdu_body[3]
            D_REGISTERS[addr] = val
            print(f">> [信捷PLC] 收到写寄存器指令: D{addr} = {val}")
            return bytes([func_code]) + pdu_body[:4]
        return bytes([func_code | 0x80, 0x03])

    # 默认回送空操作响应
    return bytes([func_code, 0x01, 0x00])


# ==========================================
# 1. 模拟信捷 PLC (Modbus TCP) - 监听 502 端口
# ==========================================
def plc_tcp_worker():
    PLC_PORT = 502
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        server.bind(('0.0.0.0', PLC_PORT))
        server.listen(5)
        print(f"[信捷PLC TCP] 模拟器已就绪，监听端口 {PLC_PORT}...")
    except Exception as e:
        print(f"[信捷PLC TCP] 启动失败: {e} (若端口被占用可忽略，正常使用串口即可)")
        return

    while True:
        client, addr = server.accept()
        def handle_client(c):
            try:
                while True:
                    req = c.recv(1024)
                    if not req or len(req) < 8:
                        break
                    # 解析 Modbus TCP MBAP Header
                    trans_id = req[0:2]
                    unit_id = req[6:7]
                    func_code = req[7]
                    pdu_body = req[8:]

                    resp_pdu = process_modbus_pdu(func_code, pdu_body)
                    mbap_len = len(resp_pdu) + 1
                    resp = trans_id + b'\x00\x00' + bytes([(mbap_len >> 8) & 0xFF, mbap_len & 0xFF]) + unit_id + resp_pdu
                    c.sendall(resp)
            except Exception:
                pass
            finally:
                c.close()

        threading.Thread(target=handle_client, args=(client,), daemon=True).start()


# ==========================================
# 2. 模拟信捷 PLC (Modbus RTU 串口)
# ==========================================
def plc_serial_rtu_worker(com_port="COM11", baud_rate=115200):
    try:
        import serial
    except ImportError:
        print(f"[信捷PLC RTU] 未检测到 pyserial 库。如需使用虚拟串口测试信捷 RTU，请执行: pip install pyserial")
        return

    try:
        ser = serial.Serial(
            port=com_port,
            baudrate=baud_rate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.1
        )
        print(f"[信捷PLC RTU] 串口模拟器已就绪，已打开串口: {com_port}，波特率: {baud_rate}...")
    except Exception as e:
        print(f"[信捷PLC RTU] 串口 {com_port} 打开失败: {e}")
        print(f"提示：可使用 VSPD 等工具创建成对虚拟串口（例如 COM10 <-> COM11），上位机连 COM10，模拟器连 COM11。")
        return

    buffer = bytearray()
    while True:
        try:
            data = ser.read(128)
            if data:
                buffer.extend(data)
                # Modbus RTU 常见请求帧长度为 8 字节 (Slave + Func + AddrH + AddrL + QtyH + QtyL + CrcL + CrcH)
                while len(buffer) >= 8:
                    slave_id = buffer[0]
                    func_code = buffer[1]
                    req_frame = buffer[:8]
                    
                    # 校验 CRC
                    expected_crc = crc16_modbus(req_frame[:6])
                    if req_frame[6:8] == expected_crc:
                        buffer = buffer[8:]
                        pdu_body = req_frame[2:6]
                        resp_pdu = process_modbus_pdu(func_code, pdu_body)
                        resp_frame = bytes([slave_id]) + resp_pdu
                        resp_frame += crc16_modbus(resp_frame)
                        ser.write(resp_frame)
                    else:
                        # CRC 不匹配，移位滑动
                        buffer.pop(0)
            else:
                time.sleep(0.01)
        except Exception as e:
            time.sleep(0.1)


# ==========================================
# 3. 模拟海康扫码枪 (TCP Server) - 监听 2001 端口
# ==========================================
def scanner_simulator_worker():
    SCANNER_PORT = 2001
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        server.bind(('0.0.0.0', SCANNER_PORT))
        server.listen(5)
        print(f"[海康扫码枪] 模拟器已就绪，监听端口 {SCANNER_PORT}...")
    except Exception as e:
        print(f"[海康扫码枪] 启动失败: {e}。")
        return

    while True:
        client, addr = server.accept()
        def handle_client(c):
            try:
                req = c.recv(1024)
                if req:
                    cmd_str = req.decode('utf-8', errors='ignore').strip()
                    print(f"\n>> [海康扫码枪] 收到上位机触发指令: '{cmd_str}'")
                    time.sleep(0.3)
                    # 返回模拟条码
                    fake_barcode = "SIM_BARCODE_2026_XWD\r\n"
                    c.sendall(fake_barcode.encode('utf-8'))
                    print(f">> [海康扫码枪] 已成功返回模拟条码: {fake_barcode.strip()}\n")
            except Exception:
                pass
            finally:
                c.close()

        threading.Thread(target=handle_client, args=(client,), daemon=True).start()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="信捷 PLC 与海康扫码枪组合模拟器")
    parser.add_argument("--com", type=str, default="COM11", help="信捷 PLC 模拟串口号（默认 COM11）")
    parser.add_argument("--baud", type=int, default=115200, help="信捷 PLC 波特率（默认 115200，可选 19200）")
    args = parser.parse_args()

    print("==================================================")
    print("       信捷 PLC & 海康扫码枪 组合模拟器")
    print("   (参考: Main功能演示_信捷PLC动态库使用演示.cs)")
    print("==================================================")

    # 1. 启动 PLC TCP 模拟 (端口 502)
    t_tcp = threading.Thread(target=plc_tcp_worker, daemon=True)
    t_tcp.start()

    # 2. 启动 信捷 PLC 串口 RTU 模拟
    t_rtu = threading.Thread(target=plc_serial_rtu_worker, args=(args.com, args.baud), daemon=True)
    t_rtu.start()

    # 3. 启动 海康扫码枪 TCP 模拟 (端口 2001)
    t_scan = threading.Thread(target=scanner_simulator_worker, daemon=True)
    t_scan.start()

    time.sleep(0.5)

    print("\n--------------------------------------------------")
    print("【使用说明】:")
    print("1. 上位机中设置监听信捷 PLC 线圈 (例如 M100 或 X0)。")
    print("2. 在控制台直接按【回车键 (Enter)】，模拟工人拍下启动按键：")
    print("   -> 模拟器将向查询的线圈返回 ON (1)；")
    print("   -> 上位机检测到后自动触发扫码枪 (2001 端口) 收到条码并开测；")
    print("   -> 模拟器自动复位信号为 0 (模拟按键弹起)。")
    print("3. 输入 'q' 并回车可退出模拟器。")
    print("--------------------------------------------------\n")

    try:
        while True:
            user_in = input("提示: [按回车键 = 模拟按键按下触发开测] > ").strip()
            if user_in.lower() == 'q':
                break

            PLC_TRIGGER = True
            print("=> [信捷PLC] 按键线圈已置为 ON (1)，等待上位机轮询读取...\n")
    except KeyboardInterrupt:
        print("\n模拟器已安全关闭。")
