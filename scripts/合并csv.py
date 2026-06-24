import pandas as pd

input_file = r"C:\Users\Administrator\Downloads\2026-06-17_自由测试工站   V1.1.14报告.xls"
output_file = "merged.csv"

# 1️⃣ 读取 xls
df = pd.read_excel(input_file, engine="xlrd")

# 2️⃣ 清理列名（防止空格/乱码）
df.columns = df.columns.str.strip()

# 3️⃣ 按 SN 分组
result = []

# 定义所有需要提取的测试项
test_items = [
    "通知治具开始测量静态电流（CC）",
    "等待治具数据长包-机号",
    "等待治具数据长包-静态电流(uA)",
    "等待治具数据长包-工作电流(mA)",
    "等待治具数据长包-充电电流(mA)",
    "等待治具数据长包-泵电压(mV)",
    "等待治具数据长包-MCU电压(mV)",
    "等待治具数据长包-阀电压(mV)",
    "MES启动",
    "扫描连接蓝牙",
    "M8 LED全开",
    "M8读取版本号",
    "M8获取电量信息",
    "M8查询电池温度",
    "M8开启按键上报",
    "M8检查模式切换",
    "M8检查减挡位",
    "M8检查加挡位",
    "通知治具准备开始测试（AA）",
    "M8吸力测试开",
    "通知治具开始测量工作电流（EE）",
    "等待治具工作电流测量完成",
    "M8吸力测试关",
    "M8关机"
]

for sn, group in df.groupby("sn"):
    base = group.iloc[0]

    items = {}
    for _, row in group.iterrows():
        items[str(row["测试项"])] = f'{row["测试数据"]}({row["测试结果"]})'

    # 构建结果字典
    row_result = {
        "SN": sn,
        "版本": base["上位机版本"],
        "MAC": base["mac地址"],
        "时间": base["时间戳"],
        "结果": "FAIL" if "失败" in group["测试结果"].values else "PASS"
    }
    
    # 添加所有测试项
    for item in test_items:
        row_result[item] = items.get(item, "")
    
    result.append(row_result)

# 4️⃣ 输出 CSV
out_df = pd.DataFrame(result)
out_df.to_csv(output_file, index=False, encoding="utf-8-sig")

print("done ->", output_file)