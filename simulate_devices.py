import socket
import threading
import time
import sys

# 全局变量
PLC_TRIGGER = False

# ==========================================
# 1. 模拟 PLC (Modbus TCP) - 监听 502 端口
# ==========================================
def plc_simulator_worker():
    global PLC_TRIGGER
    PLC_PORT = 502
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # 允许端口复用
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        server.bind(('0.0.0.0', PLC_PORT))
        server.listen(5)
        print(f"[PLC] 模拟器已启动，正在监听端口 {PLC_PORT}...")
    except Exception as e:
        print(f"[PLC] 启动失败: {e}。请确保没有其他程序占用 502 端口，并且以管理员权限运行。")
        return

    while True:
        client, addr = server.accept()
        # print(f"[PLC] 上位机已连接: {addr}")
        
        def handle_client(c):
            global PLC_TRIGGER
            try:
                while True:
                    req = c.recv(1024)
                    if not req:
                        break
                    # 简单解析 Modbus TCP 报文
                    if len(req) >= 12:
                        trans_id = req[0:2]
                        unit_id = req[6:7]
                        func_code = req[7]
                        
                        # 如果是读线圈 (0x01) 或 读离散输入 (0x02)
                        if func_code in (1, 2):
                            # 根据当前的触发状态返回
                            state_val = 0x01 if PLC_TRIGGER else 0x00
                            resp = trans_id + b'\x00\x00\x00\x04' + unit_id + bytes([func_code, 0x01, state_val])
                            c.sendall(resp)
                            
                            # 如果刚刚触发了上位机，立刻将信号复位（模拟按键弹起）
                            if PLC_TRIGGER:
                                print(">> [PLC] 检测到上位机查询，已下发触发信号 (ON) 并自动复位！\n")
                                PLC_TRIGGER = False
            except Exception as e:
                pass
            finally:
                c.close()

        threading.Thread(target=handle_client, args=(client,), daemon=True).start()


# ==========================================
# 2. 模拟海康扫码枪 (TCP Server) - 监听 2001 端口
# ==========================================
def scanner_simulator_worker():
    SCANNER_PORT = 2001
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        server.bind(('0.0.0.0', SCANNER_PORT))
        server.listen(5)
        print(f"[扫码枪] 模拟器已启动，正在监听端口 {SCANNER_PORT}...")
    except Exception as e:
        print(f"[扫码枪] 启动失败: {e}。")
        return

    while True:
        client, addr = server.accept()
        # print(f"[扫码枪] 上位机已连接: {addr}")
        
        def handle_client(c):
            try:
                req = c.recv(1024)
                if req:
                    cmd_str = req.decode('utf-8', errors='ignore').strip()
                    print(f"\n>> [扫码枪] 收到上位机指令: '{cmd_str}'")
                    
                    # 假装正在扫码，延迟 0.5 秒
                    time.sleep(0.5)
                    
                    # 发送模拟条码（长度需满足解析要求）
                    fake_barcode = "SIM_BARCODE_2026_XWD\r\n"
                    c.sendall(fake_barcode.encode('utf-8'))
                    print(f">> [扫码枪] 已成功返回条码: {fake_barcode.strip()}\n")
            except Exception as e:
                pass
            finally:
                c.close()

        threading.Thread(target=handle_client, args=(client,), daemon=True).start()


if __name__ == "__main__":
    print("========================================")
    print("      PLC & 扫码枪 组合模拟器 (可控版)")
    print("========================================")
    
    t1 = threading.Thread(target=plc_simulator_worker, daemon=True)
    t2 = threading.Thread(target=scanner_simulator_worker, daemon=True)
    
    t1.start()
    t2.start()
    
    # 稍等一下让端口打印出来
    time.sleep(0.5)
    
    print("\n----------------------------------------")
    print("【操作说明】:")
    print("在下面直接按回车键 (Enter)，即可向 PLC 发送一个启动信号 (ON)。")
    print("按 Ctrl+C 或输入 q 退出模拟器。")
    print("----------------------------------------\n")
    
    # 保持主线程用来接收用户输入
    try:
        while True:
            # 兼容 python 2/3 的输入
            if sys.version_info[0] < 3:
                user_in = raw_input("提示: [直接回车=发PLC启动信号] > ")
            else:
                user_in = input("提示: [直接回车=发PLC启动信号] > ")
                
            user_in = user_in.strip()
            if user_in.lower() == 'q':
                break
            
            PLC_TRIGGER = True
            print("=> PLC 信号已置为 ON，等待上位机读取中...\n")
    except KeyboardInterrupt:
        print("\n模拟器已关闭。")
