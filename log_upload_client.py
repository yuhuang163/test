#!/usr/bin/env python3
"""
Log 文件上传客户端
用于向 https://fctp.luteos.com/api/factory-tool/logs/upload 上传 log 文件
"""
import argparse
import urllib3

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
import requests


def upload_log_file(file_path, deviceId, station="", url="https://fctp.luteos.com/api/factory-tool/logs/upload"):
    """上传 log 文件到指定接口"""
    try:
        with open(file_path, 'rb') as file:
            files = {'file': file}
            # 补充Form必填参数
            form_data = {
                "deviceId": deviceId,
                "station": station
            }
            print(f"正在上传文件: {file_path}")
            print(f"目标地址: {url}")

            # data传表单、files传文件
            response = requests.post(url, data=form_data, files=files, verify=False)

            print(f"HTTP 状态码: {response.status_code}")
            print(f"响应内容: {response.text}")

            try:
                json_response = response.json()
                if json_response.get('code') == 0:
                    print("✓ 文件上传成功！")
                else:
                    print(f"✗ 上传失败: {json_response.get('message', '未知错误')}")
            except ValueError:
                print("响应不是 JSON 格式")

            return response

    except FileNotFoundError:
        print(f"✗ 文件不存在: {file_path}")
        return None
    except requests.exceptions.RequestException as e:
        print(f"✗ 请求失败: {e}")
        return None


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='上传 log 文件到工厂工具服务')
    file = 'DESKTOP-P4ULNMS_上位机日志_default_2026-06-02.txt'
    url = 'https://fctp.luteos.com/api/factory-tool/logs/upload'
    # 填写本机设备ID
    dev_id = "DESKTOP-P4ULNMS"
    upload_log_file(file, deviceId=dev_id, station="DEFAULT")
