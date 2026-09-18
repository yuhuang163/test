// Auto-generated from mainwindow.ui — do not edit
window.UI_PREVIEW_DATA = {
  "source": "mainwindow.ui",
  "header_connection_area": {
    "groupBox_5": {
      "objectName": "groupBox_5",
      "title": "1、设备连接",
      "controls": [
        {
          "type": "QPushButton",
          "objectName": "connectButton",
          "text": "连接Dongle"
        },
        {
          "type": "QPushButton",
          "objectName": "disconnectButton",
          "text": "断开Dongle"
        },
        {
          "type": "QCheckBox",
          "objectName": "is_reset_dongle",
          "text": "连接后自动复位Dongle"
        },
        {
          "type": "QPushButton",
          "objectName": "start_search",
          "text": "开始搜索设备"
        },
        {
          "type": "QPushButton",
          "objectName": "connectProductButton",
          "text": "连接产品"
        },
        {
          "type": "QPushButton",
          "objectName": "pushButton",
          "text": "测试(开发用)"
        }
      ],
      "button_rows": [
        [
          "连接Dongle",
          "断开Dongle",
          "开始搜索设备",
          "连接产品",
          "测试(开发用)"
        ]
      ],
      "fields": [
        {
          "label": "Dongle串口：",
          "objectName": "label_dongle_serial",
          "field": {
            "type": "QComboBox",
            "objectName": "comNameCombo"
          }
        },
        {
          "field": {
            "type": "QCheckBox",
            "objectName": "is_reset_dongle",
            "text": "连接后自动复位Dongle"
          }
        },
        {
          "label": "已发现设备：",
          "objectName": "label_pick_device",
          "field": {
            "type": "QComboBox",
            "objectName": "pick_device"
          }
        },
        {
          "label": "名称筛选：",
          "objectName": "label_name_range",
          "field": {
            "type": "QComboBox",
            "objectName": "name_range"
          }
        },
        {
          "label": "最低信号：",
          "objectName": "label_rssi_range"
        },
        {
          "label": "-60 dBm",
          "objectName": "rssi_range_value"
        },
        {
          "label": "蓝牙MAC连接：",
          "objectName": "label_2"
        },
        {
          "field": {
            "type": "QLineEdit",
            "objectName": "macInput",
            "placeholder": "扫描/输入MAC后连接"
          }
        }
      ]
    },
    "verticalGroupBox": {
      "objectName": "verticalGroupBox",
      "title": "2、设备信息绑定",
      "controls": [
        {
          "type": "QCheckBox",
          "objectName": "checkbanding",
          "text": "仅绑定SN，不读取版本"
        }
      ],
      "button_rows": [],
      "fields": [
        {
          "label": "SN取MAC：",
          "objectName": "label_5",
          "field": {
            "type": "QLineEdit",
            "objectName": "getMac",
            "placeholder": "扫描/输入SN，回车后从文件获取MAC"
          }
        },
        {
          "label": "绑定SN：",
          "objectName": "label_6",
          "field": {
            "type": "QLineEdit",
            "objectName": "snInput",
            "placeholder": "扫描/输入需要绑定的SN"
          }
        },
        {
          "field": {
            "type": "QCheckBox",
            "objectName": "checkbanding",
            "text": "仅绑定SN，不读取版本"
          }
        }
      ]
    }
  },
  "tabWidget": {
    "objectName": "tabWidget",
    "tabs": [
      {
        "objectName": "tab_device_info",
        "title": "基础/外设",
        "content": [
          {
            "type": "QSplitter",
            "objectName": "splitter",
            "parts": [
              {
                "type": "QGroupBox",
                "objectName": "groupBox_11",
                "title": "基础属性",
                "button_rows": [],
                "controls": [
                  {
                    "type": "QPushButton",
                    "objectName": "getBasicInfoButton",
                    "text": "测试基础信息"
                  }
                ],
                "fields": [],
                "tables": [
                  {
                    "type": "QTableView",
                    "objectName": "baseView",
                    "note": "[表格: baseView]"
                  }
                ],
                "children": []
              },
              {
                "type": "QGroupBox",
                "objectName": "groupBox_2",
                "title": "外设状态",
                "button_rows": [],
                "controls": [
                  {
                    "type": "QPushButton",
                    "objectName": "getperipheralButton",
                    "text": "测试外设"
                  }
                ],
                "fields": [],
                "tables": [
                  {
                    "type": "QTableView",
                    "objectName": "peripheralView",
                    "note": "[表格: peripheralView]"
                  }
                ],
                "children": []
              }
            ]
          }
        ]
      },
      {
        "objectName": "tab_debug_group_business",
        "title": "产品业务控制",
        "content": [
          {
            "type": "QTabWidget",
            "objectName": "tabWidget_debug_business",
            "tabs": [
              {
                "objectName": "tab_business_control",
                "title": "业务控制",
                "content": [
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_business_control",
                    "title": "产品业务控制",
                    "button_rows": [
                      [
                        "进入工厂模式",
                        "退出工厂模式",
                        "关机",
                        "重启",
                        "恢复出厂设置",
                        "产测完成写入",
                        "产测未完成写入",
                        "产测完成读取",
                        "获取当前曲目"
                      ],
                      [
                        "写入整机的sn",
                        "写入板子sn",
                        "获取整机的sn",
                        "获取板子sn"
                      ],
                      [
                        "进入亮白模式",
                        "清洁模式",
                        "进入休眠模式",
                        "开启刷牙流程",
                        "禁止休眠模式"
                      ],
                      [
                        "进入吸力测试",
                        "退出吸力测试",
                        "吸力补偿开",
                        "吸力补偿关"
                      ]
                    ],
                    "controls": [],
                    "fields": [
                      {
                        "label": "设置设备模式",
                        "objectName": "label_107",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "set_mode"
                        }
                      },
                      {
                        "field": {
                          "type": "QCheckBox",
                          "objectName": "show_bursh_log",
                          "text": "串口指令模式是否打开"
                        }
                      },
                      {
                        "label": "模式",
                        "objectName": "label_qaiot_fac_mode",
                        "field": {
                          "type": "QComboBox",
                          "objectName": "qaiot_fac_mode_type"
                        }
                      }
                    ],
                    "tables": [],
                    "children": [
                      {
                        "type": "QGroupBox",
                        "objectName": "groupBox_root_suction_test",
                        "title": "吸力测试(0x81)",
                        "button_rows": [],
                        "controls": [
                          {
                            "type": "QLabel",
                            "objectName": "label_root_suction_switch",
                            "text": "开关"
                          },
                          {
                            "type": "QComboBox",
                            "objectName": "root_suction_switch",
                            "items": [
                              "关",
                              "开"
                            ]
                          },
                          {
                            "type": "QLabel",
                            "objectName": "label_root_suction_mode",
                            "text": "模式"
                          },
                          {
                            "type": "QComboBox",
                            "objectName": "root_suction_mode",
                            "items": [
                              "按摩模式",
                              "吸乳模式",
                              "混合模式"
                            ]
                          },
                          {
                            "type": "QLabel",
                            "objectName": "label_root_suction_level",
                            "text": "强度"
                          },
                          {
                            "type": "QSpinBox",
                            "objectName": "root_suction_level"
                          },
                          {
                            "type": "QPushButton",
                            "objectName": "send_root_suction_test",
                            "text": "发送吸力测试"
                          }
                        ],
                        "fields": [],
                        "tables": [],
                        "children": []
                      },
                      {
                        "type": "QGroupBox",
                        "objectName": "groupBox_6",
                        "title": "设备元数据",
                        "button_rows": [
                          [
                            "写入mac地址",
                            "写入设备id",
                            "写入产品id",
                            "写入三元组密钥"
                          ],
                          [
                            "获取mac地址",
                            "获取设备id",
                            "获取产品id",
                            "获取三元组信息"
                          ]
                        ],
                        "controls": [],
                        "fields": [
                          {
                            "label": "三元密钥",
                            "objectName": "label_109",
                            "field": {
                              "type": "QLineEdit",
                              "objectName": "kTlvKey_data"
                            }
                          }
                        ],
                        "tables": [],
                        "children": []
                      }
                    ]
                  }
                ]
              }
            ]
          }
        ],
        "tabWidget_debug_business": {
          "type": "QTabWidget",
          "objectName": "tabWidget_debug_business",
          "tabs": [
            {
              "objectName": "tab_business_control",
              "title": "业务控制",
              "content": [
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_business_control",
                  "title": "产品业务控制",
                  "button_rows": [
                    [
                      "进入工厂模式",
                      "退出工厂模式",
                      "关机",
                      "重启",
                      "恢复出厂设置",
                      "产测完成写入",
                      "产测未完成写入",
                      "产测完成读取",
                      "获取当前曲目"
                    ],
                    [
                      "写入整机的sn",
                      "写入板子sn",
                      "获取整机的sn",
                      "获取板子sn"
                    ],
                    [
                      "进入亮白模式",
                      "清洁模式",
                      "进入休眠模式",
                      "开启刷牙流程",
                      "禁止休眠模式"
                    ],
                    [
                      "进入吸力测试",
                      "退出吸力测试",
                      "吸力补偿开",
                      "吸力补偿关"
                    ]
                  ],
                  "controls": [],
                  "fields": [
                    {
                      "label": "设置设备模式",
                      "objectName": "label_107",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "set_mode"
                      }
                    },
                    {
                      "field": {
                        "type": "QCheckBox",
                        "objectName": "show_bursh_log",
                        "text": "串口指令模式是否打开"
                      }
                    },
                    {
                      "label": "模式",
                      "objectName": "label_qaiot_fac_mode",
                      "field": {
                        "type": "QComboBox",
                        "objectName": "qaiot_fac_mode_type"
                      }
                    }
                  ],
                  "tables": [],
                  "children": [
                    {
                      "type": "QGroupBox",
                      "objectName": "groupBox_root_suction_test",
                      "title": "吸力测试(0x81)",
                      "button_rows": [],
                      "controls": [
                        {
                          "type": "QLabel",
                          "objectName": "label_root_suction_switch",
                          "text": "开关"
                        },
                        {
                          "type": "QComboBox",
                          "objectName": "root_suction_switch",
                          "items": [
                            "关",
                            "开"
                          ]
                        },
                        {
                          "type": "QLabel",
                          "objectName": "label_root_suction_mode",
                          "text": "模式"
                        },
                        {
                          "type": "QComboBox",
                          "objectName": "root_suction_mode",
                          "items": [
                            "按摩模式",
                            "吸乳模式",
                            "混合模式"
                          ]
                        },
                        {
                          "type": "QLabel",
                          "objectName": "label_root_suction_level",
                          "text": "强度"
                        },
                        {
                          "type": "QSpinBox",
                          "objectName": "root_suction_level"
                        },
                        {
                          "type": "QPushButton",
                          "objectName": "send_root_suction_test",
                          "text": "发送吸力测试"
                        }
                      ],
                      "fields": [],
                      "tables": [],
                      "children": []
                    },
                    {
                      "type": "QGroupBox",
                      "objectName": "groupBox_6",
                      "title": "设备元数据",
                      "button_rows": [
                        [
                          "写入mac地址",
                          "写入设备id",
                          "写入产品id",
                          "写入三元组密钥"
                        ],
                        [
                          "获取mac地址",
                          "获取设备id",
                          "获取产品id",
                          "获取三元组信息"
                        ]
                      ],
                      "controls": [],
                      "fields": [
                        {
                          "label": "三元密钥",
                          "objectName": "label_109",
                          "field": {
                            "type": "QLineEdit",
                            "objectName": "kTlvKey_data"
                          }
                        }
                      ],
                      "tables": [],
                      "children": []
                    }
                  ]
                }
              ]
            }
          ]
        }
      },
      {
        "objectName": "tab_debug_group_peripheral",
        "title": "外设",
        "content": [
          {
            "type": "QTabWidget",
            "objectName": "tabWidget_debug_peripheral",
            "tabs": [
              {
                "objectName": "screenInspectPage",
                "title": "屏幕",
                "content": [
                  {
                    "kind": "button_row",
                    "buttons": [
                      "切换屏幕颜色"
                    ]
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_screenInspectCamera",
                    "title": "摄像头",
                    "button_rows": [],
                    "controls": [],
                    "fields": [
                      {
                        "label": "来源",
                        "objectName": "label_cameraSource",
                        "field": {
                          "type": "QComboBox",
                          "objectName": "comboBox_cameraSource",
                          "items": [
                            "USB",
                            "GigE"
                          ]
                        }
                      },
                      {
                        "label": "设备",
                        "objectName": "label_camera",
                        "field": {
                          "type": "QComboBox",
                          "objectName": "comboBox_camera"
                        }
                      },
                      {
                        "label": "IP",
                        "objectName": "label_gigeIp",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "lineEdit_gigeIp",
                          "placeholder": "169.254.64.10"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_screenInspectColor",
                    "title": "坏点算法期望纯色（不发设备）",
                    "button_rows": [],
                    "controls": [
                      {
                        "type": "QPushButton",
                        "objectName": "btnColorBlue",
                        "text": "蓝"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "btnColorGreen",
                        "text": "绿"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "btnColorRed",
                        "text": "红"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "btnColorWhite",
                        "text": "白"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "btnColorBlack",
                        "text": "黑"
                      },
                      {
                        "type": "QLabel",
                        "objectName": "label_expectedColor",
                        "text": "期望画面"
                      },
                      {
                        "type": "QComboBox",
                        "objectName": "comboBox_expectedColor"
                      }
                    ],
                    "fields": [],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_screenInspectAction",
                    "title": "采集与判定",
                    "button_rows": [],
                    "controls": [],
                    "fields": [
                      {
                        "field": {
                          "type": "QCheckBox",
                          "objectName": "checkBox_autoInspect",
                          "text": "采集后自动检测"
                        }
                      },
                      {
                        "label": "最小相似度",
                        "objectName": "label_minSsim",
                        "field": {
                          "type": "QDoubleSpinBox",
                          "objectName": "doubleSpinBox_minSsim"
                        }
                      },
                      {
                        "label": "坏点残差",
                        "objectName": "label_deadDiff",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "spinBox_deadDiff"
                        }
                      },
                      {
                        "label": "最大坏点数",
                        "objectName": "label_maxDead",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "spinBox_maxDead"
                        }
                      },
                      {
                        "label": "亮度不均上限",
                        "objectName": "label_mura",
                        "field": {
                          "type": "QDoubleSpinBox",
                          "objectName": "doubleSpinBox_mura"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_refImage",
                    "title": "参考图",
                    "button_rows": [],
                    "controls": [
                      {
                        "type": "QLabel",
                        "objectName": "label_refImage",
                        "text": "未加载"
                      }
                    ],
                    "fields": [],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_currImage",
                    "title": "拍摄图（拖拽划定检测范围，红圈为疑似坏点）",
                    "button_rows": [],
                    "controls": [
                      {
                        "type": "QLabel",
                        "objectName": "label_currImage",
                        "text": "未采集"
                      }
                    ],
                    "fields": [],
                    "tables": [],
                    "children": []
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "相似度：--",
                        "objectName": "label_screenInspectSimilarity"
                      },
                      {
                        "label": "结果：等待检测",
                        "objectName": "label_screenInspectVerdict"
                      }
                    ]
                  },
                  {
                    "type": "QPlainTextEdit",
                    "objectName": "plainTextEdit_screenInspectLog",
                    "placeholder": "检测详情"
                  },
                  {
                    "kind": "button_row",
                    "buttons": [
                      "背光开启",
                      "背光关闭"
                    ]
                  }
                ]
              },
              {
                "objectName": "tab_periph_vibrator",
                "title": "振子",
                "content": [
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_qaiot_vibrator",
                    "title": "Qaiot 振子",
                    "button_rows": [],
                    "controls": [],
                    "fields": [
                      {
                        "label": "振动强度",
                        "objectName": "label_qaiot_vib_str",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_vib_str"
                        }
                      },
                      {
                        "label": "频",
                        "objectName": "label_qaiot_vib_freq",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_vib_freq"
                        }
                      },
                      {
                        "label": "时长",
                        "objectName": "label_qaiot_vib_dur",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_vib_dur"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  }
                ]
              },
              {
                "objectName": "tab_periph_heat",
                "title": "加热",
                "content": [
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_qaiot_heat",
                    "title": "Qaiot 加热",
                    "button_rows": [],
                    "controls": [],
                    "fields": [
                      {
                        "label": "加热强度",
                        "objectName": "label_qaiot_heat_str",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_heat_str"
                        }
                      },
                      {
                        "label": "时长",
                        "objectName": "label_qaiot_heat_dur",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_heat_dur"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_root_heat_level",
                    "title": "加热(0x83/0x98)",
                    "button_rows": [
                      [
                        "查询加热温度"
                      ]
                    ],
                    "controls": [],
                    "fields": [
                      {
                        "label": "开关",
                        "objectName": "label_root_heat_switch",
                        "field": {
                          "type": "QComboBox",
                          "objectName": "root_heat_switch",
                          "items": [
                            "关",
                            "开"
                          ]
                        }
                      },
                      {
                        "label": "档位",
                        "objectName": "label_root_heat_level",
                        "field": {
                          "type": "QComboBox",
                          "objectName": "root_heat_level",
                          "items": [
                            "L1",
                            "L2",
                            "L3"
                          ]
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  }
                ]
              },
              {
                "objectName": "tab_periph_battery",
                "title": "电池",
                "content": [
                  {
                    "type": "QGroupBox",
                    "objectName": "verticalGroupBox_4",
                    "title": "电池测试",
                    "button_rows": [],
                    "controls": [
                      {
                        "type": "QPushButton",
                        "objectName": "get_battery",
                        "text": "显示充电情况"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "get_root_battery_temp",
                        "text": "查询电池温度"
                      },
                      {
                        "type": "QLabel",
                        "objectName": "root_battery_temp_value",
                        "text": "电池温度："
                      },
                      {
                        "type": "QLabel",
                        "objectName": "battary_voltage",
                        "text": "电压为:"
                      },
                      {
                        "type": "QLabel",
                        "objectName": "battary_state",
                        "text": "充电状态："
                      },
                      {
                        "type": "QLabel",
                        "objectName": "battary_value",
                        "text": "电量为："
                      },
                      {
                        "type": "QLabel",
                        "objectName": "battery_type",
                        "text": "电池类型："
                      },
                      {
                        "type": "QLineEdit",
                        "objectName": "charge_current_ma",
                        "placeholder": "mA"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "set_current_charge",
                        "text": "设置充电电流"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "read_current_charge",
                        "text": "读取充电电流"
                      },
                      {
                        "type": "QComboBox",
                        "objectName": "battery_type_combox"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "set_battery",
                        "text": "设置电池类型"
                      }
                    ],
                    "fields": [],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_qaiot_batt",
                    "title": "Qaiot 模拟电池",
                    "button_rows": [],
                    "controls": [],
                    "fields": [
                      {
                        "label": "模拟电池%",
                        "objectName": "label_qaiot_batt_percent",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_batt_percent"
                        }
                      },
                      {
                        "label": "mV",
                        "objectName": "label_qaiot_batt_mv",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_batt_mv"
                        }
                      },
                      {
                        "label": "mA",
                        "objectName": "label_qaiot_batt_ma",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_batt_ma"
                        }
                      },
                      {
                        "label": "℃",
                        "objectName": "label_qaiot_batt_temp",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_batt_temp"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  }
                ]
              },
              {
                "objectName": "tab_periph_key",
                "title": "按键",
                "content": [
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_qaiot_key",
                    "title": "Qaiot 模拟按键",
                    "button_rows": [],
                    "controls": [],
                    "fields": [
                      {
                        "label": "模拟按键",
                        "objectName": "label_qaiot_sim_key",
                        "field": {
                          "type": "QComboBox",
                          "objectName": "qaiot_sim_key"
                        }
                      },
                      {
                        "label": "读模式状态",
                        "objectName": "label_qaiot_aging_mode",
                        "field": {
                          "type": "QComboBox",
                          "objectName": "qaiot_aging_mode"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QPushButton",
                    "objectName": "get_keysignal",
                    "text": "读取按键电容"
                  }
                ]
              },
              {
                "objectName": "tab_5",
                "title": "电机",
                "content": [
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_101",
                    "title": "P20PRO电机",
                    "button_rows": [],
                    "controls": [],
                    "fields": [
                      {
                        "label": "电机校准参数",
                        "objectName": "label_12",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "motor_cali_param"
                        }
                      },
                      {
                        "label": "振动频率",
                        "objectName": "label_8",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "vibrate_freq"
                        }
                      },
                      {
                        "label": "振动角度",
                        "objectName": "label_9",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "vibrate_angle"
                        }
                      },
                      {
                        "label": "摆动频率",
                        "objectName": "label_10",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "sweeping_freq"
                        }
                      },
                      {
                        "label": "摆动角度",
                        "objectName": "label_11",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "sweeping_angle"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "verticalGroupBox_2",
                    "title": "Y20/Y20PRO电机",
                    "button_rows": [],
                    "controls": [],
                    "fields": [
                      {
                        "label": "频率",
                        "objectName": "label_7",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "fre",
                          "placeholder": "范围100~1000"
                        }
                      },
                      {
                        "label": "占空比",
                        "objectName": "lab",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "duty",
                          "placeholder": "范围0~100"
                        }
                      },
                      {
                        "field": {
                          "type": "QCheckBox",
                          "objectName": "open_motor_set",
                          "text": "是否设好参数自动开启电机"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  },
                  {
                    "kind": "button_row",
                    "buttons": [
                      "获取电机信息",
                      "摆幅测试",
                      "关闭电机校准流程",
                      "开启电机校准流程",
                      "阻尼开",
                      "阻尼关",
                      "电机测试开",
                      "电机测试关"
                    ]
                  },
                  {
                    "kind": "button_row",
                    "buttons": [
                      "霍尔和零点校准",
                      "电机重定位",
                      "打开电机adc开关",
                      "关闭电机adc开关"
                    ]
                  }
                ]
              },
              {
                "objectName": "tab_periph_imu",
                "title": "陀螺仪",
                "content": [
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_100",
                    "title": "IMU测试",
                    "button_rows": [
                      [
                        "(旧版本)六轴校准",
                        "停止六轴校准",
                        "开启六轴采集（处理）",
                        "关闭六轴采集（处理）",
                        "转化xls",
                        "开启六轴采集",
                        "关闭六轴采集"
                      ],
                      [
                        "设置校准信息",
                        "获取校准信息"
                      ]
                    ],
                    "controls": [
                      {
                        "type": "QLabel",
                        "objectName": "imutimestamp",
                        "text": "timestamp="
                      }
                    ],
                    "fields": [
                      {
                        "label": "gyro_x=",
                        "objectName": "gyro_x"
                      },
                      {
                        "label": "gyro_y=",
                        "objectName": "gyro_y"
                      },
                      {
                        "label": "gyro_z=",
                        "objectName": "gyro_z"
                      },
                      {
                        "label": "acc_x=",
                        "objectName": "acc_x"
                      },
                      {
                        "label": "acc_y=",
                        "objectName": "acc_y"
                      },
                      {
                        "label": "acc_z=",
                        "objectName": "acc_z"
                      },
                      {
                        "label": "kx",
                        "objectName": "label_71",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "kx"
                        }
                      },
                      {
                        "label": "ky",
                        "objectName": "label_72",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "ky"
                        }
                      },
                      {
                        "label": "kz",
                        "objectName": "label_73",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "kz"
                        }
                      },
                      {
                        "label": "bx",
                        "objectName": "label_77",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "bx"
                        }
                      },
                      {
                        "label": "by",
                        "objectName": "label_78",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "by"
                        }
                      },
                      {
                        "label": "bz",
                        "objectName": "label_79",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "bz"
                        }
                      },
                      {
                        "label": "szx",
                        "objectName": "label_75",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "szx"
                        }
                      },
                      {
                        "label": "szy",
                        "objectName": "label_76",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "szy"
                        }
                      },
                      {
                        "label": "syx",
                        "objectName": "label_74",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "syx"
                        }
                      },
                      {
                        "label": "gyro_x",
                        "objectName": "label_80",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "gyro_x_2"
                        }
                      },
                      {
                        "label": "gyro_y",
                        "objectName": "label_81",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "gyro_y_2"
                        }
                      },
                      {
                        "label": "gyro_z",
                        "objectName": "label_82",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "gyro_z_2"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  }
                ]
              },
              {
                "objectName": "tab_11",
                "title": "摄像头",
                "content": [
                  {
                    "type": "QSplitter",
                    "objectName": "splitter_4",
                    "parts": [
                      {
                        "type": "QGroupBox",
                        "objectName": "verticalGroupBox_9",
                        "title": "",
                        "button_rows": [],
                        "controls": [],
                        "fields": [],
                        "tables": [],
                        "children": [
                          {
                            "type": "QGroupBox",
                            "objectName": "verticalGroupBox_10",
                            "title": "Wi-Fi 配网",
                            "button_rows": [],
                            "controls": [
                              {
                                "type": "QLineEdit",
                                "objectName": "port_num"
                              },
                              {
                                "type": "QPushButton",
                                "objectName": "distribution_network",
                                "text": "配网"
                              },
                              {
                                "type": "QLabel",
                                "objectName": "password_label",
                                "text": "PASSWORD:"
                              },
                              {
                                "type": "QLineEdit",
                                "objectName": "ssid_lineEdit"
                              },
                              {
                                "type": "QLineEdit",
                                "objectName": "password_lineEdit"
                              },
                              {
                                "type": "QLabel",
                                "objectName": "IP_TEXT",
                                "text": "本机IP地址："
                              },
                              {
                                "type": "QLabel",
                                "objectName": "ssid_label",
                                "text": "SSID:"
                              },
                              {
                                "type": "QLabel",
                                "objectName": "port_set_label",
                                "text": "设置端口号："
                              },
                              {
                                "type": "QPushButton",
                                "objectName": "py_test",
                                "text": "脚本测试"
                              },
                              {
                                "type": "QComboBox",
                                "objectName": "ip_comboBox"
                              },
                              {
                                "type": "QPushButton",
                                "objectName": "closeconnect",
                                "text": "断开服务器"
                              }
                            ],
                            "fields": [],
                            "tables": [],
                            "children": []
                          }
                        ]
                      },
                      {
                        "type": "QGroupBox",
                        "objectName": "horizontalGroupBox_5",
                        "title": "摄像头实时画面",
                        "button_rows": [],
                        "controls": [],
                        "fields": [
                          {
                            "field": {
                              "type": "QCheckBox",
                              "objectName": "quicklz_picture",
                              "text": "压缩"
                            }
                          }
                        ],
                        "tables": [],
                        "children": []
                      }
                    ]
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_21",
                    "title": "相机参数设置",
                    "button_rows": [],
                    "controls": [],
                    "fields": [
                      {
                        "label": "曝光时间：",
                        "objectName": "exposure_time_label",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "exposure_time_edit",
                          "placeholder": "ms"
                        }
                      }
                    ],
                    "tables": [],
                    "children": [
                      {
                        "type": "QGroupBox",
                        "objectName": "horizontalGroupBox_2",
                        "title": "调试参数",
                        "button_rows": [],
                        "controls": [
                          {
                            "type": "QLabel",
                            "objectName": "timestamp_label",
                            "text": "时间戳："
                          },
                          {
                            "type": "QLabel",
                            "objectName": "timestamp_value",
                            "text": "Null"
                          },
                          {
                            "type": "QLabel",
                            "objectName": "label_166",
                            "text": "接收到的数据："
                          },
                          {
                            "type": "QLabel",
                            "objectName": "label_167",
                            "text": "Null"
                          },
                          {
                            "type": "QLabel",
                            "objectName": "label_168",
                            "text": "接收到数据包长度："
                          },
                          {
                            "type": "QLabel",
                            "objectName": "label_169",
                            "text": "Null"
                          }
                        ],
                        "fields": [],
                        "tables": [],
                        "children": []
                      }
                    ]
                  }
                ]
              },
              {
                "objectName": "tab_13",
                "title": "LED灯",
                "content": [
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_screen_lights",
                    "title": "灯光分区",
                    "button_rows": [
                      [
                        "白灯开关",
                        "红灯开关",
                        "RGB灯光测试"
                      ]
                    ],
                    "controls": [
                      {
                        "type": "QLabel",
                        "objectName": "yedengslider_value",
                        "text": "0/100"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "night_brightness",
                        "text": "夜灯亮度"
                      }
                    ],
                    "fields": [
                      {
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "light1"
                        }
                      },
                      {
                        "label": "左上灯区",
                        "objectName": "label_58"
                      },
                      {
                        "label": "R1",
                        "objectName": "label_55"
                      },
                      {
                        "label": "0",
                        "objectName": "label_46"
                      },
                      {
                        "label": "G1",
                        "objectName": "label_56"
                      },
                      {
                        "label": "0",
                        "objectName": "label_47"
                      },
                      {
                        "label": "B1",
                        "objectName": "label_57"
                      },
                      {
                        "label": "0",
                        "objectName": "label_48",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "light2"
                        }
                      },
                      {
                        "label": "右上灯区",
                        "objectName": "label_59"
                      },
                      {
                        "label": "R2",
                        "objectName": "label_52"
                      },
                      {
                        "label": "0",
                        "objectName": "label_62"
                      },
                      {
                        "label": "G2",
                        "objectName": "label_53"
                      },
                      {
                        "label": "0",
                        "objectName": "label_63"
                      },
                      {
                        "label": "B2",
                        "objectName": "label_54"
                      },
                      {
                        "label": "0",
                        "objectName": "label_64",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "light3"
                        }
                      },
                      {
                        "label": "左下灯区",
                        "objectName": "label_60"
                      },
                      {
                        "label": "R3",
                        "objectName": "label_49"
                      },
                      {
                        "label": "0",
                        "objectName": "label_65"
                      },
                      {
                        "label": "G3",
                        "objectName": "label_50"
                      },
                      {
                        "label": "0",
                        "objectName": "label_66"
                      },
                      {
                        "label": "B3",
                        "objectName": "label_51"
                      },
                      {
                        "label": "0",
                        "objectName": "label_67",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "light4"
                        }
                      },
                      {
                        "label": "右下灯区",
                        "objectName": "label_61"
                      },
                      {
                        "label": "R4",
                        "objectName": "label_43"
                      },
                      {
                        "label": "0",
                        "objectName": "label_68"
                      },
                      {
                        "label": "G4",
                        "objectName": "label_44"
                      },
                      {
                        "label": "0",
                        "objectName": "label_69"
                      },
                      {
                        "label": "B4",
                        "objectName": "label_45"
                      },
                      {
                        "label": "0",
                        "objectName": "label_70"
                      }
                    ],
                    "tables": [],
                    "children": []
                  }
                ]
              },
              {
                "objectName": "tab_periph_ble",
                "title": "蓝牙",
                "content": [
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_3",
                    "title": "Wi-Fi 与蓝牙测试",
                    "button_rows": [],
                    "controls": [],
                    "fields": [],
                    "tables": [],
                    "children": [
                      {
                        "type": "QGroupBox",
                        "objectName": "groupBox_wifi_net",
                        "title": "配网与 RSSI 显示",
                        "button_rows": [],
                        "controls": [],
                        "fields": [
                          {
                            "label": "Wi-Fi 账号",
                            "objectName": "label_3",
                            "field": {
                              "type": "QLineEdit",
                              "objectName": "wifiUserName"
                            }
                          },
                          {
                            "label": "Wi-Fi 密码",
                            "objectName": "label_4",
                            "field": {
                              "type": "QLineEdit",
                              "objectName": "wifiPassword"
                            }
                          },
                          {
                            "label": "蓝牙 RSSI：",
                            "objectName": "BLE_RSSI"
                          },
                          {
                            "label": "Wi-Fi RSSI：",
                            "objectName": "WIFI_RSSI"
                          }
                        ],
                        "tables": [],
                        "children": []
                      },
                      {
                        "type": "QGroupBox",
                        "objectName": "groupBox_wifi_connect_actions",
                        "title": "连接与测量",
                        "button_rows": [],
                        "controls": [
                          {
                            "type": "QCheckBox",
                            "objectName": "show_rssi",
                            "text": "显示蓝牙 RSSI"
                          },
                          {
                            "type": "QPushButton",
                            "objectName": "get_rssi_device",
                            "text": "获取对端经典蓝牙 RSSI"
                          },
                          {
                            "type": "QPushButton",
                            "objectName": "get_ble_rssi_device",
                            "text": "获取对端 BLE RSSI"
                          },
                          {
                            "type": "QPushButton",
                            "objectName": "connectwifi",
                            "text": "连接 Wi-Fi"
                          },
                          {
                            "type": "QPushButton",
                            "objectName": "disconnectwifi",
                            "text": "断开 Wi-Fi"
                          },
                          {
                            "type": "QPushButton",
                            "objectName": "getwifi",
                            "text": "获取 Wi-Fi 设置信息"
                          },
                          {
                            "type": "QPushButton",
                            "objectName": "new_connectwifi",
                            "text": "连接 Wi-Fi（新接口）"
                          }
                        ],
                        "fields": [],
                        "tables": [],
                        "children": []
                      }
                    ]
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_signal_debug",
                    "title": "调试（慎用）",
                    "button_rows": [],
                    "controls": [
                      {
                        "type": "QPushButton",
                        "objectName": "get_trim_data",
                        "text": "获取trim值"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "set_trim_data",
                        "text": "设置trim值"
                      },
                      {
                        "type": "QLineEdit",
                        "objectName": "trim_data_line"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "enter_ble_cali",
                        "text": "进入蓝牙校频"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "exit_ble_cali",
                        "text": "退出蓝牙校频"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "enter_ble_test",
                        "text": "进入蓝牙信令测试"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "exit_ble_test",
                        "text": "退出蓝牙信令测试"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "enter_no_ble_test",
                        "text": "进入蓝牙非信令测试"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "exit_no_ble_test",
                        "text": "退出蓝牙非信令测试"
                      }
                    ],
                    "fields": [],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QPushButton",
                    "objectName": "start_wifible_test",
                    "text": "开始 Wi-Fi / 蓝牙信号测试"
                  },
                  {
                    "type": "QLabel",
                    "objectName": "ble_test_result",
                    "text": "WAIT"
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_wifi_test_result",
                    "title": "Wi-Fi 测试结果",
                    "button_rows": [],
                    "controls": [],
                    "fields": [
                      {
                        "label": "WAIT",
                        "objectName": "wifi_test_result"
                      }
                    ],
                    "tables": [],
                    "children": []
                  }
                ]
              },
              {
                "objectName": "tab_periph_pump",
                "title": "泵",
                "content": [
                  {
                    "type": "QWidget",
                    "objectName": "widget_pump_stall",
                    "content": [
                      {
                        "type": "QLabel",
                        "objectName": "root_pump_stall_value",
                        "text": "泵堵电流：未读取"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "get_root_pump_stall",
                        "text": "读取泵堵电流"
                      }
                    ]
                  },
                  {
                    "type": "QWidget",
                    "objectName": "widget_flange",
                    "content": [
                      {
                        "type": "QLabel",
                        "objectName": "root_flange_value",
                        "text": "法兰状态：未检测"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "get_root_flange",
                        "text": "法兰在位检测"
                      }
                    ]
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_qaiot_pump",
                    "title": "Qaiot 泵参数",
                    "button_rows": [],
                    "controls": [],
                    "fields": [
                      {
                        "label": "泵",
                        "objectName": "label_qaiot_pump",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_pump_circle"
                        }
                      },
                      {
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_pump_dur"
                        }
                      },
                      {
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_pump_int"
                        }
                      },
                      {
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_pump_pwm"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  }
                ]
              },
              {
                "objectName": "tab_periph_valve",
                "title": "阀",
                "content": [
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_qaiot_valve",
                    "title": "Qaiot 阀参数",
                    "button_rows": [],
                    "controls": [],
                    "fields": [
                      {
                        "label": "阀",
                        "objectName": "label_qaiot_valve",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_valve_en"
                        }
                      },
                      {
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_valve_dis"
                        }
                      },
                      {
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_valve_pwm"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  }
                ]
              },
              {
                "objectName": "tab_periph_light",
                "title": "光感",
                "content": [
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_light_sensor_test",
                    "title": "光感测试",
                    "button_rows": [],
                    "controls": [
                      {
                        "type": "QPushButton",
                        "objectName": "light_repo_start",
                        "text": "开启光感上报"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "light_repo_stop",
                        "text": "关闭光感上报"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "set_light_sensor",
                        "text": "设置光照校准值"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "read_light_sensor",
                        "text": "读取光照校准值"
                      }
                    ],
                    "fields": [],
                    "tables": [],
                    "children": []
                  }
                ]
              },
              {
                "objectName": "tab_periph_pressure",
                "title": "压感",
                "content": [
                  {
                    "type": "QGroupBox",
                    "objectName": "verticalGroupBox_1",
                    "title": "压感测试",
                    "button_rows": [
                      [
                        "获取压感校准信息",
                        "设置压感校准信息"
                      ],
                      [
                        "开启按键上报",
                        "关闭按键上报"
                      ]
                    ],
                    "controls": [],
                    "fields": [
                      {
                        "label": "模式按键adc：",
                        "objectName": "mode_botton_adc"
                      },
                      {
                        "label": "模式按键压力：",
                        "objectName": "mode_botton_value"
                      },
                      {
                        "label": "电机adc：",
                        "objectName": "brush_adc"
                      },
                      {
                        "label": "电机压力：",
                        "objectName": "brush_value"
                      },
                      {
                        "label": "电源按键adc：",
                        "objectName": "power_botton_adc"
                      },
                      {
                        "label": "电源按键压力：",
                        "objectName": "power_botton_value"
                      },
                      {
                        "label": "辅助元件adc：",
                        "objectName": "assistant_botton_adc"
                      },
                      {
                        "label": "辅助元件压力：",
                        "objectName": "assistant_botton_value"
                      },
                      {
                        "label": "电机校准值",
                        "objectName": "label_brushHeadCalib",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "calibValue_brushHead"
                        }
                      },
                      {
                        "label": "辅助元件校准值",
                        "objectName": "label_auxComponentCalib",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "calibValue_auxComponent"
                        }
                      },
                      {
                        "label": "模式按键校准值",
                        "objectName": "label_modeButtonCalib",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "calibValue_modeButton"
                        }
                      },
                      {
                        "label": "电源按键校准值",
                        "objectName": "label_powerButtonCalib",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "calibValue_powerButton"
                        }
                      },
                      {
                        "label": "温度校准值",
                        "objectName": "label_brushHeadCalib_2",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "calibValue_temp"
                        }
                      },
                      {
                        "field": {
                          "type": "QCheckBox",
                          "objectName": "press_sensor_temp__switch",
                          "text": "压感温度补偿开关"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  }
                ]
              }
            ]
          }
        ],
        "tabWidget_debug_peripheral": {
          "type": "QTabWidget",
          "objectName": "tabWidget_debug_peripheral",
          "tabs": [
            {
              "objectName": "screenInspectPage",
              "title": "屏幕",
              "content": [
                {
                  "kind": "button_row",
                  "buttons": [
                    "切换屏幕颜色"
                  ]
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_screenInspectCamera",
                  "title": "摄像头",
                  "button_rows": [],
                  "controls": [],
                  "fields": [
                    {
                      "label": "来源",
                      "objectName": "label_cameraSource",
                      "field": {
                        "type": "QComboBox",
                        "objectName": "comboBox_cameraSource",
                        "items": [
                          "USB",
                          "GigE"
                        ]
                      }
                    },
                    {
                      "label": "设备",
                      "objectName": "label_camera",
                      "field": {
                        "type": "QComboBox",
                        "objectName": "comboBox_camera"
                      }
                    },
                    {
                      "label": "IP",
                      "objectName": "label_gigeIp",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "lineEdit_gigeIp",
                        "placeholder": "169.254.64.10"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_screenInspectColor",
                  "title": "坏点算法期望纯色（不发设备）",
                  "button_rows": [],
                  "controls": [
                    {
                      "type": "QPushButton",
                      "objectName": "btnColorBlue",
                      "text": "蓝"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "btnColorGreen",
                      "text": "绿"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "btnColorRed",
                      "text": "红"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "btnColorWhite",
                      "text": "白"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "btnColorBlack",
                      "text": "黑"
                    },
                    {
                      "type": "QLabel",
                      "objectName": "label_expectedColor",
                      "text": "期望画面"
                    },
                    {
                      "type": "QComboBox",
                      "objectName": "comboBox_expectedColor"
                    }
                  ],
                  "fields": [],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_screenInspectAction",
                  "title": "采集与判定",
                  "button_rows": [],
                  "controls": [],
                  "fields": [
                    {
                      "field": {
                        "type": "QCheckBox",
                        "objectName": "checkBox_autoInspect",
                        "text": "采集后自动检测"
                      }
                    },
                    {
                      "label": "最小相似度",
                      "objectName": "label_minSsim",
                      "field": {
                        "type": "QDoubleSpinBox",
                        "objectName": "doubleSpinBox_minSsim"
                      }
                    },
                    {
                      "label": "坏点残差",
                      "objectName": "label_deadDiff",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "spinBox_deadDiff"
                      }
                    },
                    {
                      "label": "最大坏点数",
                      "objectName": "label_maxDead",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "spinBox_maxDead"
                      }
                    },
                    {
                      "label": "亮度不均上限",
                      "objectName": "label_mura",
                      "field": {
                        "type": "QDoubleSpinBox",
                        "objectName": "doubleSpinBox_mura"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_refImage",
                  "title": "参考图",
                  "button_rows": [],
                  "controls": [
                    {
                      "type": "QLabel",
                      "objectName": "label_refImage",
                      "text": "未加载"
                    }
                  ],
                  "fields": [],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_currImage",
                  "title": "拍摄图（拖拽划定检测范围，红圈为疑似坏点）",
                  "button_rows": [],
                  "controls": [
                    {
                      "type": "QLabel",
                      "objectName": "label_currImage",
                      "text": "未采集"
                    }
                  ],
                  "fields": [],
                  "tables": [],
                  "children": []
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "相似度：--",
                      "objectName": "label_screenInspectSimilarity"
                    },
                    {
                      "label": "结果：等待检测",
                      "objectName": "label_screenInspectVerdict"
                    }
                  ]
                },
                {
                  "type": "QPlainTextEdit",
                  "objectName": "plainTextEdit_screenInspectLog",
                  "placeholder": "检测详情"
                },
                {
                  "kind": "button_row",
                  "buttons": [
                    "背光开启",
                    "背光关闭"
                  ]
                }
              ]
            },
            {
              "objectName": "tab_periph_vibrator",
              "title": "振子",
              "content": [
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_qaiot_vibrator",
                  "title": "Qaiot 振子",
                  "button_rows": [],
                  "controls": [],
                  "fields": [
                    {
                      "label": "振动强度",
                      "objectName": "label_qaiot_vib_str",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_vib_str"
                      }
                    },
                    {
                      "label": "频",
                      "objectName": "label_qaiot_vib_freq",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_vib_freq"
                      }
                    },
                    {
                      "label": "时长",
                      "objectName": "label_qaiot_vib_dur",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_vib_dur"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                }
              ]
            },
            {
              "objectName": "tab_periph_heat",
              "title": "加热",
              "content": [
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_qaiot_heat",
                  "title": "Qaiot 加热",
                  "button_rows": [],
                  "controls": [],
                  "fields": [
                    {
                      "label": "加热强度",
                      "objectName": "label_qaiot_heat_str",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_heat_str"
                      }
                    },
                    {
                      "label": "时长",
                      "objectName": "label_qaiot_heat_dur",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_heat_dur"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_root_heat_level",
                  "title": "加热(0x83/0x98)",
                  "button_rows": [
                    [
                      "查询加热温度"
                    ]
                  ],
                  "controls": [],
                  "fields": [
                    {
                      "label": "开关",
                      "objectName": "label_root_heat_switch",
                      "field": {
                        "type": "QComboBox",
                        "objectName": "root_heat_switch",
                        "items": [
                          "关",
                          "开"
                        ]
                      }
                    },
                    {
                      "label": "档位",
                      "objectName": "label_root_heat_level",
                      "field": {
                        "type": "QComboBox",
                        "objectName": "root_heat_level",
                        "items": [
                          "L1",
                          "L2",
                          "L3"
                        ]
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                }
              ]
            },
            {
              "objectName": "tab_periph_battery",
              "title": "电池",
              "content": [
                {
                  "type": "QGroupBox",
                  "objectName": "verticalGroupBox_4",
                  "title": "电池测试",
                  "button_rows": [],
                  "controls": [
                    {
                      "type": "QPushButton",
                      "objectName": "get_battery",
                      "text": "显示充电情况"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "get_root_battery_temp",
                      "text": "查询电池温度"
                    },
                    {
                      "type": "QLabel",
                      "objectName": "root_battery_temp_value",
                      "text": "电池温度："
                    },
                    {
                      "type": "QLabel",
                      "objectName": "battary_voltage",
                      "text": "电压为:"
                    },
                    {
                      "type": "QLabel",
                      "objectName": "battary_state",
                      "text": "充电状态："
                    },
                    {
                      "type": "QLabel",
                      "objectName": "battary_value",
                      "text": "电量为："
                    },
                    {
                      "type": "QLabel",
                      "objectName": "battery_type",
                      "text": "电池类型："
                    },
                    {
                      "type": "QLineEdit",
                      "objectName": "charge_current_ma",
                      "placeholder": "mA"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "set_current_charge",
                      "text": "设置充电电流"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "read_current_charge",
                      "text": "读取充电电流"
                    },
                    {
                      "type": "QComboBox",
                      "objectName": "battery_type_combox"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "set_battery",
                      "text": "设置电池类型"
                    }
                  ],
                  "fields": [],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_qaiot_batt",
                  "title": "Qaiot 模拟电池",
                  "button_rows": [],
                  "controls": [],
                  "fields": [
                    {
                      "label": "模拟电池%",
                      "objectName": "label_qaiot_batt_percent",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_batt_percent"
                      }
                    },
                    {
                      "label": "mV",
                      "objectName": "label_qaiot_batt_mv",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_batt_mv"
                      }
                    },
                    {
                      "label": "mA",
                      "objectName": "label_qaiot_batt_ma",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_batt_ma"
                      }
                    },
                    {
                      "label": "℃",
                      "objectName": "label_qaiot_batt_temp",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_batt_temp"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                }
              ]
            },
            {
              "objectName": "tab_periph_key",
              "title": "按键",
              "content": [
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_qaiot_key",
                  "title": "Qaiot 模拟按键",
                  "button_rows": [],
                  "controls": [],
                  "fields": [
                    {
                      "label": "模拟按键",
                      "objectName": "label_qaiot_sim_key",
                      "field": {
                        "type": "QComboBox",
                        "objectName": "qaiot_sim_key"
                      }
                    },
                    {
                      "label": "读模式状态",
                      "objectName": "label_qaiot_aging_mode",
                      "field": {
                        "type": "QComboBox",
                        "objectName": "qaiot_aging_mode"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QPushButton",
                  "objectName": "get_keysignal",
                  "text": "读取按键电容"
                }
              ]
            },
            {
              "objectName": "tab_5",
              "title": "电机",
              "content": [
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_101",
                  "title": "P20PRO电机",
                  "button_rows": [],
                  "controls": [],
                  "fields": [
                    {
                      "label": "电机校准参数",
                      "objectName": "label_12",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "motor_cali_param"
                      }
                    },
                    {
                      "label": "振动频率",
                      "objectName": "label_8",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "vibrate_freq"
                      }
                    },
                    {
                      "label": "振动角度",
                      "objectName": "label_9",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "vibrate_angle"
                      }
                    },
                    {
                      "label": "摆动频率",
                      "objectName": "label_10",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "sweeping_freq"
                      }
                    },
                    {
                      "label": "摆动角度",
                      "objectName": "label_11",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "sweeping_angle"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QGroupBox",
                  "objectName": "verticalGroupBox_2",
                  "title": "Y20/Y20PRO电机",
                  "button_rows": [],
                  "controls": [],
                  "fields": [
                    {
                      "label": "频率",
                      "objectName": "label_7",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "fre",
                        "placeholder": "范围100~1000"
                      }
                    },
                    {
                      "label": "占空比",
                      "objectName": "lab",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "duty",
                        "placeholder": "范围0~100"
                      }
                    },
                    {
                      "field": {
                        "type": "QCheckBox",
                        "objectName": "open_motor_set",
                        "text": "是否设好参数自动开启电机"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                },
                {
                  "kind": "button_row",
                  "buttons": [
                    "获取电机信息",
                    "摆幅测试",
                    "关闭电机校准流程",
                    "开启电机校准流程",
                    "阻尼开",
                    "阻尼关",
                    "电机测试开",
                    "电机测试关"
                  ]
                },
                {
                  "kind": "button_row",
                  "buttons": [
                    "霍尔和零点校准",
                    "电机重定位",
                    "打开电机adc开关",
                    "关闭电机adc开关"
                  ]
                }
              ]
            },
            {
              "objectName": "tab_periph_imu",
              "title": "陀螺仪",
              "content": [
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_100",
                  "title": "IMU测试",
                  "button_rows": [
                    [
                      "(旧版本)六轴校准",
                      "停止六轴校准",
                      "开启六轴采集（处理）",
                      "关闭六轴采集（处理）",
                      "转化xls",
                      "开启六轴采集",
                      "关闭六轴采集"
                    ],
                    [
                      "设置校准信息",
                      "获取校准信息"
                    ]
                  ],
                  "controls": [
                    {
                      "type": "QLabel",
                      "objectName": "imutimestamp",
                      "text": "timestamp="
                    }
                  ],
                  "fields": [
                    {
                      "label": "gyro_x=",
                      "objectName": "gyro_x"
                    },
                    {
                      "label": "gyro_y=",
                      "objectName": "gyro_y"
                    },
                    {
                      "label": "gyro_z=",
                      "objectName": "gyro_z"
                    },
                    {
                      "label": "acc_x=",
                      "objectName": "acc_x"
                    },
                    {
                      "label": "acc_y=",
                      "objectName": "acc_y"
                    },
                    {
                      "label": "acc_z=",
                      "objectName": "acc_z"
                    },
                    {
                      "label": "kx",
                      "objectName": "label_71",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "kx"
                      }
                    },
                    {
                      "label": "ky",
                      "objectName": "label_72",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "ky"
                      }
                    },
                    {
                      "label": "kz",
                      "objectName": "label_73",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "kz"
                      }
                    },
                    {
                      "label": "bx",
                      "objectName": "label_77",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "bx"
                      }
                    },
                    {
                      "label": "by",
                      "objectName": "label_78",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "by"
                      }
                    },
                    {
                      "label": "bz",
                      "objectName": "label_79",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "bz"
                      }
                    },
                    {
                      "label": "szx",
                      "objectName": "label_75",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "szx"
                      }
                    },
                    {
                      "label": "szy",
                      "objectName": "label_76",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "szy"
                      }
                    },
                    {
                      "label": "syx",
                      "objectName": "label_74",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "syx"
                      }
                    },
                    {
                      "label": "gyro_x",
                      "objectName": "label_80",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "gyro_x_2"
                      }
                    },
                    {
                      "label": "gyro_y",
                      "objectName": "label_81",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "gyro_y_2"
                      }
                    },
                    {
                      "label": "gyro_z",
                      "objectName": "label_82",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "gyro_z_2"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                }
              ]
            },
            {
              "objectName": "tab_11",
              "title": "摄像头",
              "content": [
                {
                  "type": "QSplitter",
                  "objectName": "splitter_4",
                  "parts": [
                    {
                      "type": "QGroupBox",
                      "objectName": "verticalGroupBox_9",
                      "title": "",
                      "button_rows": [],
                      "controls": [],
                      "fields": [],
                      "tables": [],
                      "children": [
                        {
                          "type": "QGroupBox",
                          "objectName": "verticalGroupBox_10",
                          "title": "Wi-Fi 配网",
                          "button_rows": [],
                          "controls": [
                            {
                              "type": "QLineEdit",
                              "objectName": "port_num"
                            },
                            {
                              "type": "QPushButton",
                              "objectName": "distribution_network",
                              "text": "配网"
                            },
                            {
                              "type": "QLabel",
                              "objectName": "password_label",
                              "text": "PASSWORD:"
                            },
                            {
                              "type": "QLineEdit",
                              "objectName": "ssid_lineEdit"
                            },
                            {
                              "type": "QLineEdit",
                              "objectName": "password_lineEdit"
                            },
                            {
                              "type": "QLabel",
                              "objectName": "IP_TEXT",
                              "text": "本机IP地址："
                            },
                            {
                              "type": "QLabel",
                              "objectName": "ssid_label",
                              "text": "SSID:"
                            },
                            {
                              "type": "QLabel",
                              "objectName": "port_set_label",
                              "text": "设置端口号："
                            },
                            {
                              "type": "QPushButton",
                              "objectName": "py_test",
                              "text": "脚本测试"
                            },
                            {
                              "type": "QComboBox",
                              "objectName": "ip_comboBox"
                            },
                            {
                              "type": "QPushButton",
                              "objectName": "closeconnect",
                              "text": "断开服务器"
                            }
                          ],
                          "fields": [],
                          "tables": [],
                          "children": []
                        }
                      ]
                    },
                    {
                      "type": "QGroupBox",
                      "objectName": "horizontalGroupBox_5",
                      "title": "摄像头实时画面",
                      "button_rows": [],
                      "controls": [],
                      "fields": [
                        {
                          "field": {
                            "type": "QCheckBox",
                            "objectName": "quicklz_picture",
                            "text": "压缩"
                          }
                        }
                      ],
                      "tables": [],
                      "children": []
                    }
                  ]
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_21",
                  "title": "相机参数设置",
                  "button_rows": [],
                  "controls": [],
                  "fields": [
                    {
                      "label": "曝光时间：",
                      "objectName": "exposure_time_label",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "exposure_time_edit",
                        "placeholder": "ms"
                      }
                    }
                  ],
                  "tables": [],
                  "children": [
                    {
                      "type": "QGroupBox",
                      "objectName": "horizontalGroupBox_2",
                      "title": "调试参数",
                      "button_rows": [],
                      "controls": [
                        {
                          "type": "QLabel",
                          "objectName": "timestamp_label",
                          "text": "时间戳："
                        },
                        {
                          "type": "QLabel",
                          "objectName": "timestamp_value",
                          "text": "Null"
                        },
                        {
                          "type": "QLabel",
                          "objectName": "label_166",
                          "text": "接收到的数据："
                        },
                        {
                          "type": "QLabel",
                          "objectName": "label_167",
                          "text": "Null"
                        },
                        {
                          "type": "QLabel",
                          "objectName": "label_168",
                          "text": "接收到数据包长度："
                        },
                        {
                          "type": "QLabel",
                          "objectName": "label_169",
                          "text": "Null"
                        }
                      ],
                      "fields": [],
                      "tables": [],
                      "children": []
                    }
                  ]
                }
              ]
            },
            {
              "objectName": "tab_13",
              "title": "LED灯",
              "content": [
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_screen_lights",
                  "title": "灯光分区",
                  "button_rows": [
                    [
                      "白灯开关",
                      "红灯开关",
                      "RGB灯光测试"
                    ]
                  ],
                  "controls": [
                    {
                      "type": "QLabel",
                      "objectName": "yedengslider_value",
                      "text": "0/100"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "night_brightness",
                      "text": "夜灯亮度"
                    }
                  ],
                  "fields": [
                    {
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "light1"
                      }
                    },
                    {
                      "label": "左上灯区",
                      "objectName": "label_58"
                    },
                    {
                      "label": "R1",
                      "objectName": "label_55"
                    },
                    {
                      "label": "0",
                      "objectName": "label_46"
                    },
                    {
                      "label": "G1",
                      "objectName": "label_56"
                    },
                    {
                      "label": "0",
                      "objectName": "label_47"
                    },
                    {
                      "label": "B1",
                      "objectName": "label_57"
                    },
                    {
                      "label": "0",
                      "objectName": "label_48",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "light2"
                      }
                    },
                    {
                      "label": "右上灯区",
                      "objectName": "label_59"
                    },
                    {
                      "label": "R2",
                      "objectName": "label_52"
                    },
                    {
                      "label": "0",
                      "objectName": "label_62"
                    },
                    {
                      "label": "G2",
                      "objectName": "label_53"
                    },
                    {
                      "label": "0",
                      "objectName": "label_63"
                    },
                    {
                      "label": "B2",
                      "objectName": "label_54"
                    },
                    {
                      "label": "0",
                      "objectName": "label_64",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "light3"
                      }
                    },
                    {
                      "label": "左下灯区",
                      "objectName": "label_60"
                    },
                    {
                      "label": "R3",
                      "objectName": "label_49"
                    },
                    {
                      "label": "0",
                      "objectName": "label_65"
                    },
                    {
                      "label": "G3",
                      "objectName": "label_50"
                    },
                    {
                      "label": "0",
                      "objectName": "label_66"
                    },
                    {
                      "label": "B3",
                      "objectName": "label_51"
                    },
                    {
                      "label": "0",
                      "objectName": "label_67",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "light4"
                      }
                    },
                    {
                      "label": "右下灯区",
                      "objectName": "label_61"
                    },
                    {
                      "label": "R4",
                      "objectName": "label_43"
                    },
                    {
                      "label": "0",
                      "objectName": "label_68"
                    },
                    {
                      "label": "G4",
                      "objectName": "label_44"
                    },
                    {
                      "label": "0",
                      "objectName": "label_69"
                    },
                    {
                      "label": "B4",
                      "objectName": "label_45"
                    },
                    {
                      "label": "0",
                      "objectName": "label_70"
                    }
                  ],
                  "tables": [],
                  "children": []
                }
              ]
            },
            {
              "objectName": "tab_periph_ble",
              "title": "蓝牙",
              "content": [
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_3",
                  "title": "Wi-Fi 与蓝牙测试",
                  "button_rows": [],
                  "controls": [],
                  "fields": [],
                  "tables": [],
                  "children": [
                    {
                      "type": "QGroupBox",
                      "objectName": "groupBox_wifi_net",
                      "title": "配网与 RSSI 显示",
                      "button_rows": [],
                      "controls": [],
                      "fields": [
                        {
                          "label": "Wi-Fi 账号",
                          "objectName": "label_3",
                          "field": {
                            "type": "QLineEdit",
                            "objectName": "wifiUserName"
                          }
                        },
                        {
                          "label": "Wi-Fi 密码",
                          "objectName": "label_4",
                          "field": {
                            "type": "QLineEdit",
                            "objectName": "wifiPassword"
                          }
                        },
                        {
                          "label": "蓝牙 RSSI：",
                          "objectName": "BLE_RSSI"
                        },
                        {
                          "label": "Wi-Fi RSSI：",
                          "objectName": "WIFI_RSSI"
                        }
                      ],
                      "tables": [],
                      "children": []
                    },
                    {
                      "type": "QGroupBox",
                      "objectName": "groupBox_wifi_connect_actions",
                      "title": "连接与测量",
                      "button_rows": [],
                      "controls": [
                        {
                          "type": "QCheckBox",
                          "objectName": "show_rssi",
                          "text": "显示蓝牙 RSSI"
                        },
                        {
                          "type": "QPushButton",
                          "objectName": "get_rssi_device",
                          "text": "获取对端经典蓝牙 RSSI"
                        },
                        {
                          "type": "QPushButton",
                          "objectName": "get_ble_rssi_device",
                          "text": "获取对端 BLE RSSI"
                        },
                        {
                          "type": "QPushButton",
                          "objectName": "connectwifi",
                          "text": "连接 Wi-Fi"
                        },
                        {
                          "type": "QPushButton",
                          "objectName": "disconnectwifi",
                          "text": "断开 Wi-Fi"
                        },
                        {
                          "type": "QPushButton",
                          "objectName": "getwifi",
                          "text": "获取 Wi-Fi 设置信息"
                        },
                        {
                          "type": "QPushButton",
                          "objectName": "new_connectwifi",
                          "text": "连接 Wi-Fi（新接口）"
                        }
                      ],
                      "fields": [],
                      "tables": [],
                      "children": []
                    }
                  ]
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_signal_debug",
                  "title": "调试（慎用）",
                  "button_rows": [],
                  "controls": [
                    {
                      "type": "QPushButton",
                      "objectName": "get_trim_data",
                      "text": "获取trim值"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "set_trim_data",
                      "text": "设置trim值"
                    },
                    {
                      "type": "QLineEdit",
                      "objectName": "trim_data_line"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "enter_ble_cali",
                      "text": "进入蓝牙校频"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "exit_ble_cali",
                      "text": "退出蓝牙校频"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "enter_ble_test",
                      "text": "进入蓝牙信令测试"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "exit_ble_test",
                      "text": "退出蓝牙信令测试"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "enter_no_ble_test",
                      "text": "进入蓝牙非信令测试"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "exit_no_ble_test",
                      "text": "退出蓝牙非信令测试"
                    }
                  ],
                  "fields": [],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QPushButton",
                  "objectName": "start_wifible_test",
                  "text": "开始 Wi-Fi / 蓝牙信号测试"
                },
                {
                  "type": "QLabel",
                  "objectName": "ble_test_result",
                  "text": "WAIT"
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_wifi_test_result",
                  "title": "Wi-Fi 测试结果",
                  "button_rows": [],
                  "controls": [],
                  "fields": [
                    {
                      "label": "WAIT",
                      "objectName": "wifi_test_result"
                    }
                  ],
                  "tables": [],
                  "children": []
                }
              ]
            },
            {
              "objectName": "tab_periph_pump",
              "title": "泵",
              "content": [
                {
                  "type": "QWidget",
                  "objectName": "widget_pump_stall",
                  "content": [
                    {
                      "type": "QLabel",
                      "objectName": "root_pump_stall_value",
                      "text": "泵堵电流：未读取"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "get_root_pump_stall",
                      "text": "读取泵堵电流"
                    }
                  ]
                },
                {
                  "type": "QWidget",
                  "objectName": "widget_flange",
                  "content": [
                    {
                      "type": "QLabel",
                      "objectName": "root_flange_value",
                      "text": "法兰状态：未检测"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "get_root_flange",
                      "text": "法兰在位检测"
                    }
                  ]
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_qaiot_pump",
                  "title": "Qaiot 泵参数",
                  "button_rows": [],
                  "controls": [],
                  "fields": [
                    {
                      "label": "泵",
                      "objectName": "label_qaiot_pump",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_pump_circle"
                      }
                    },
                    {
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_pump_dur"
                      }
                    },
                    {
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_pump_int"
                      }
                    },
                    {
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_pump_pwm"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                }
              ]
            },
            {
              "objectName": "tab_periph_valve",
              "title": "阀",
              "content": [
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_qaiot_valve",
                  "title": "Qaiot 阀参数",
                  "button_rows": [],
                  "controls": [],
                  "fields": [
                    {
                      "label": "阀",
                      "objectName": "label_qaiot_valve",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_valve_en"
                      }
                    },
                    {
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_valve_dis"
                      }
                    },
                    {
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_valve_pwm"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                }
              ]
            },
            {
              "objectName": "tab_periph_light",
              "title": "光感",
              "content": [
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_light_sensor_test",
                  "title": "光感测试",
                  "button_rows": [],
                  "controls": [
                    {
                      "type": "QPushButton",
                      "objectName": "light_repo_start",
                      "text": "开启光感上报"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "light_repo_stop",
                      "text": "关闭光感上报"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "set_light_sensor",
                      "text": "设置光照校准值"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "read_light_sensor",
                      "text": "读取光照校准值"
                    }
                  ],
                  "fields": [],
                  "tables": [],
                  "children": []
                }
              ]
            },
            {
              "objectName": "tab_periph_pressure",
              "title": "压感",
              "content": [
                {
                  "type": "QGroupBox",
                  "objectName": "verticalGroupBox_1",
                  "title": "压感测试",
                  "button_rows": [
                    [
                      "获取压感校准信息",
                      "设置压感校准信息"
                    ],
                    [
                      "开启按键上报",
                      "关闭按键上报"
                    ]
                  ],
                  "controls": [],
                  "fields": [
                    {
                      "label": "模式按键adc：",
                      "objectName": "mode_botton_adc"
                    },
                    {
                      "label": "模式按键压力：",
                      "objectName": "mode_botton_value"
                    },
                    {
                      "label": "电机adc：",
                      "objectName": "brush_adc"
                    },
                    {
                      "label": "电机压力：",
                      "objectName": "brush_value"
                    },
                    {
                      "label": "电源按键adc：",
                      "objectName": "power_botton_adc"
                    },
                    {
                      "label": "电源按键压力：",
                      "objectName": "power_botton_value"
                    },
                    {
                      "label": "辅助元件adc：",
                      "objectName": "assistant_botton_adc"
                    },
                    {
                      "label": "辅助元件压力：",
                      "objectName": "assistant_botton_value"
                    },
                    {
                      "label": "电机校准值",
                      "objectName": "label_brushHeadCalib",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "calibValue_brushHead"
                      }
                    },
                    {
                      "label": "辅助元件校准值",
                      "objectName": "label_auxComponentCalib",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "calibValue_auxComponent"
                      }
                    },
                    {
                      "label": "模式按键校准值",
                      "objectName": "label_modeButtonCalib",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "calibValue_modeButton"
                      }
                    },
                    {
                      "label": "电源按键校准值",
                      "objectName": "label_powerButtonCalib",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "calibValue_powerButton"
                      }
                    },
                    {
                      "label": "温度校准值",
                      "objectName": "label_brushHeadCalib_2",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "calibValue_temp"
                      }
                    },
                    {
                      "field": {
                        "type": "QCheckBox",
                        "objectName": "press_sensor_temp__switch",
                        "text": "压感温度补偿开关"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                }
              ]
            }
          ]
        }
      },
      {
        "objectName": "tab_debug_group_dongle",
        "title": "Dongle",
        "content": [
          {
            "type": "QTabWidget",
            "objectName": "tabWidget_debug_dongle",
            "tabs": [
              {
                "objectName": "tab_dongle_suction",
                "title": "吸力专项测试",
                "content": [
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_dongle_suction_peak_params",
                    "title": "峰检参数",
                    "button_rows": [],
                    "controls": [
                      {
                        "type": "QLabel",
                        "objectName": "label_dongle_suction_peak_dip_start",
                        "text": "计频线(kPa)"
                      },
                      {
                        "type": "QDoubleSpinBox",
                        "objectName": "dongleSuctionPeakToleranceSpin"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "btnDongleExportSuctionPng",
                        "text": "导出吸力曲线PNG"
                      },
                      {
                        "type": "QLabel",
                        "objectName": "label_dongle_suction_peak_tolerance",
                        "text": "峰值容差(kPa)"
                      },
                      {
                        "type": "QDoubleSpinBox",
                        "objectName": "dongleSuctionPeakMaxGapSpin"
                      },
                      {
                        "type": "QDoubleSpinBox",
                        "objectName": "dongleSuctionPeakTargetSpin"
                      },
                      {
                        "type": "QLabel",
                        "objectName": "label_dongle_suction_peak_max_gap",
                        "text": "漏峰间隔(s)"
                      },
                      {
                        "type": "QLabel",
                        "objectName": "label_dongle_suction_peak_target",
                        "text": "目标峰值(kPa)"
                      },
                      {
                        "type": "QDoubleSpinBox",
                        "objectName": "dongleSuctionPeakDipStartSpin"
                      }
                    ],
                    "fields": [
                      {
                        "label": "OSR档位",
                        "objectName": "label_dongle_suction_osr",
                        "field": {
                          "type": "QComboBox",
                          "objectName": "dongleSuctionOsrCombo",
                          "items": [
                            "1 · 1024X / 8ms",
                            "2 · 2048X / 10ms",
                            "3 · 4096X / 11ms",
                            "4 · 8192X / 16ms（默认）"
                          ]
                        }
                      },
                      {
                        "label": "X范围(s)",
                        "objectName": "label_dongle_suction_x_window",
                        "field": {
                          "type": "QDoubleSpinBox",
                          "objectName": "dongleSuctionXWindowSpin"
                        }
                      },
                      {
                        "label": "X刻度(s)",
                        "objectName": "label_dongle_suction_x_tick",
                        "field": {
                          "type": "QDoubleSpinBox",
                          "objectName": "dongleSuctionXTickSpin"
                        }
                      },
                      {
                        "label": "Y下限(kPa)",
                        "objectName": "label_dongle_suction_y_min",
                        "field": {
                          "type": "QDoubleSpinBox",
                          "objectName": "dongleSuctionYMinSpin"
                        }
                      },
                      {
                        "label": "Y上限(kPa)",
                        "objectName": "label_dongle_suction_y_max",
                        "field": {
                          "type": "QDoubleSpinBox",
                          "objectName": "dongleSuctionYMaxSpin"
                        }
                      },
                      {
                        "label": "Y刻度(kPa)",
                        "objectName": "label_dongle_suction_y_tick",
                        "field": {
                          "type": "QDoubleSpinBox",
                          "objectName": "dongleSuctionYTickSpin"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_dongle_at_pump",
                    "title": "泵/阀参数设置",
                    "button_rows": [
                      [
                        "设置全部内容",
                        "开始交替运行",
                        "立即停止"
                      ]
                    ],
                    "controls": [],
                    "fields": [
                      {
                        "label": "驱动方式",
                        "objectName": "label_dongle_at_drive_type",
                        "field": {
                          "type": "QComboBox",
                          "objectName": "comboDongleDriveType",
                          "items": [
                            "泵阀驱动",
                            "直线电机驱动"
                          ]
                        }
                      },
                      {
                        "label": "日志上报间隔(ms)",
                        "objectName": "label_dongle_at_fg_print",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "DongleAtFgPrint"
                        }
                      },
                      {
                        "label": "每轮段数",
                        "objectName": "label_dongle_at_seg_count",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "spinDongleSegmentCount"
                        }
                      },
                      {
                        "label": "频率：-- 次/分钟",
                        "objectName": "label_dongle_at_pump_rate"
                      },
                      {
                        "label": "总运行(ms，0=不限)",
                        "objectName": "label_dongle_at_pump_total",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "DongleAtPumpTotal"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QLabel",
                    "objectName": "dongleSuctionPacketIntervalLabel",
                    "text": "包间隔：-- ms"
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "CH1 实时-- 最高-- 最低-- 有效0 漏峰0 弱峰0 最大峰-- 频率--",
                        "objectName": "dongleSuctionCh1PeakStatLabel"
                      },
                      {
                        "label": "CH2 实时-- 最高-- 最低-- 有效0 漏峰0 弱峰0 最大峰-- 频率--",
                        "objectName": "dongleSuctionCh2PeakStatLabel"
                      },
                      {
                        "label": "CH3 实时-- 最高-- 最低-- 有效0 漏峰0 弱峰0 最大峰-- 频率--",
                        "objectName": "dongleSuctionCh3PeakStatLabel"
                      }
                    ]
                  },
                  {
                    "type": "QCheckBox",
                    "objectName": "dongleSuctionPeakGuideLinesCheck",
                    "text": "显示峰检辅助线"
                  },
                  {
                    "type": "QTableWidget",
                    "objectName": "tableDongleSegments",
                    "note": "[表格: tableDongleSegments]"
                  }
                ]
              },
              {
                "objectName": "tab_dongle_at",
                "title": "AT 测试",
                "content": [
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_dongle_at_connect",
                    "title": "连接模式（MAC 使用顶部 macInput）",
                    "button_rows": [],
                    "controls": [
                      {
                        "type": "QPushButton",
                        "objectName": "btnDongleAtDcon",
                        "text": "AT+DCON 直连"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "btnDongleAtOta",
                        "text": "AT+OTA 连接"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "btnDongleAtBle",
                        "text": "AT+BLE 连接"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "btnDongleAtMain",
                        "text": "AT+MAIN 连接"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "btnDongleAtDisconnect",
                        "text": "断开 BLE"
                      }
                    ],
                    "fields": [],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_dongle_at_passthrough",
                    "title": "透传与参数",
                    "button_rows": [],
                    "controls": [
                      {
                        "type": "QCheckBox",
                        "objectName": "chkDongleAtOtaData",
                        "text": "AT+OTADATA"
                      },
                      {
                        "type": "QCheckBox",
                        "objectName": "chkDongleAtMainData",
                        "text": "AT+MAINDATA"
                      },
                      {
                        "type": "QLabel",
                        "objectName": "label_dongle_at_ble_mtu",
                        "text": "BLE MTU(字节)"
                      },
                      {
                        "type": "QLineEdit",
                        "objectName": "DongleAtBleMtuSize"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "btnDongleAtSetBleMtu",
                        "text": "设置BLE MTU"
                      },
                      {
                        "type": "QLabel",
                        "objectName": "label_dongle_at_ota_pkt_size",
                        "text": "OTA切包(字节)"
                      },
                      {
                        "type": "QLineEdit",
                        "objectName": "DongleAtOtaPktSize"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "btnDongleAtSetOtaPkt",
                        "text": "设置切包"
                      }
                    ],
                    "fields": [],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_dongle_at_log",
                    "title": "日志",
                    "button_rows": [],
                    "controls": [
                      {
                        "type": "QCheckBox",
                        "objectName": "chkDongleAtBleLog",
                        "text": "AT+BLERSSILOG"
                      },
                      {
                        "type": "QCheckBox",
                        "objectName": "chkDongleAtBleDeviceLog",
                        "text": "AT+BLEDEVICELOG"
                      }
                    ],
                    "fields": [],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_dongle_at_adc",
                    "title": "ADC 采样",
                    "button_rows": [],
                    "controls": [
                      {
                        "type": "QCheckBox",
                        "objectName": "checkBox_adcSwitch",
                        "text": "AT+HSADC 高量程采样"
                      },
                      {
                        "type": "QCheckBox",
                        "objectName": "checkBox_lsAdcSwitch",
                        "text": "AT+LSADC 低量程采样"
                      }
                    ],
                    "fields": [],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_dongle_at_bomb",
                    "title": "船运炸弹 AT+BOMB",
                    "button_rows": [],
                    "controls": [
                      {
                        "type": "QPushButton",
                        "objectName": "ship_bomb",
                        "text": "船运炸弹"
                      }
                    ],
                    "fields": [
                      {
                        "label": "信号强度",
                        "objectName": "label_86",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "bombrssi"
                        }
                      },
                      {
                        "label": "设备名字",
                        "objectName": "label_85",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "bombname"
                        }
                      },
                      {
                        "label": "连接间隔",
                        "objectName": "label_87",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "bombinterval"
                        }
                      },
                      {
                        "label": "指令",
                        "objectName": "label_bomb_command",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "bombcommand"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_dongle_at_query",
                    "title": "查询 / 自定义 AT",
                    "button_rows": [],
                    "controls": [
                      {
                        "type": "QPushButton",
                        "objectName": "btnDongleAtGmac",
                        "text": "查询 GMAC"
                      },
                      {
                        "type": "QLineEdit",
                        "objectName": "lineDongleAtCustom",
                        "placeholder": "自定义 AT 行，例如 AT+BLEMTU=247（可不写末尾 CRLF）"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "btnDongleAtSendCustom",
                        "text": "发送"
                      }
                    ],
                    "fields": [],
                    "tables": [],
                    "children": []
                  }
                ]
              }
            ]
          }
        ],
        "tabWidget_debug_dongle": {
          "type": "QTabWidget",
          "objectName": "tabWidget_debug_dongle",
          "tabs": [
            {
              "objectName": "tab_dongle_suction",
              "title": "吸力专项测试",
              "content": [
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_dongle_suction_peak_params",
                  "title": "峰检参数",
                  "button_rows": [],
                  "controls": [
                    {
                      "type": "QLabel",
                      "objectName": "label_dongle_suction_peak_dip_start",
                      "text": "计频线(kPa)"
                    },
                    {
                      "type": "QDoubleSpinBox",
                      "objectName": "dongleSuctionPeakToleranceSpin"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "btnDongleExportSuctionPng",
                      "text": "导出吸力曲线PNG"
                    },
                    {
                      "type": "QLabel",
                      "objectName": "label_dongle_suction_peak_tolerance",
                      "text": "峰值容差(kPa)"
                    },
                    {
                      "type": "QDoubleSpinBox",
                      "objectName": "dongleSuctionPeakMaxGapSpin"
                    },
                    {
                      "type": "QDoubleSpinBox",
                      "objectName": "dongleSuctionPeakTargetSpin"
                    },
                    {
                      "type": "QLabel",
                      "objectName": "label_dongle_suction_peak_max_gap",
                      "text": "漏峰间隔(s)"
                    },
                    {
                      "type": "QLabel",
                      "objectName": "label_dongle_suction_peak_target",
                      "text": "目标峰值(kPa)"
                    },
                    {
                      "type": "QDoubleSpinBox",
                      "objectName": "dongleSuctionPeakDipStartSpin"
                    }
                  ],
                  "fields": [
                    {
                      "label": "OSR档位",
                      "objectName": "label_dongle_suction_osr",
                      "field": {
                        "type": "QComboBox",
                        "objectName": "dongleSuctionOsrCombo",
                        "items": [
                          "1 · 1024X / 8ms",
                          "2 · 2048X / 10ms",
                          "3 · 4096X / 11ms",
                          "4 · 8192X / 16ms（默认）"
                        ]
                      }
                    },
                    {
                      "label": "X范围(s)",
                      "objectName": "label_dongle_suction_x_window",
                      "field": {
                        "type": "QDoubleSpinBox",
                        "objectName": "dongleSuctionXWindowSpin"
                      }
                    },
                    {
                      "label": "X刻度(s)",
                      "objectName": "label_dongle_suction_x_tick",
                      "field": {
                        "type": "QDoubleSpinBox",
                        "objectName": "dongleSuctionXTickSpin"
                      }
                    },
                    {
                      "label": "Y下限(kPa)",
                      "objectName": "label_dongle_suction_y_min",
                      "field": {
                        "type": "QDoubleSpinBox",
                        "objectName": "dongleSuctionYMinSpin"
                      }
                    },
                    {
                      "label": "Y上限(kPa)",
                      "objectName": "label_dongle_suction_y_max",
                      "field": {
                        "type": "QDoubleSpinBox",
                        "objectName": "dongleSuctionYMaxSpin"
                      }
                    },
                    {
                      "label": "Y刻度(kPa)",
                      "objectName": "label_dongle_suction_y_tick",
                      "field": {
                        "type": "QDoubleSpinBox",
                        "objectName": "dongleSuctionYTickSpin"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_dongle_at_pump",
                  "title": "泵/阀参数设置",
                  "button_rows": [
                    [
                      "设置全部内容",
                      "开始交替运行",
                      "立即停止"
                    ]
                  ],
                  "controls": [],
                  "fields": [
                    {
                      "label": "驱动方式",
                      "objectName": "label_dongle_at_drive_type",
                      "field": {
                        "type": "QComboBox",
                        "objectName": "comboDongleDriveType",
                        "items": [
                          "泵阀驱动",
                          "直线电机驱动"
                        ]
                      }
                    },
                    {
                      "label": "日志上报间隔(ms)",
                      "objectName": "label_dongle_at_fg_print",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "DongleAtFgPrint"
                      }
                    },
                    {
                      "label": "每轮段数",
                      "objectName": "label_dongle_at_seg_count",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "spinDongleSegmentCount"
                      }
                    },
                    {
                      "label": "频率：-- 次/分钟",
                      "objectName": "label_dongle_at_pump_rate"
                    },
                    {
                      "label": "总运行(ms，0=不限)",
                      "objectName": "label_dongle_at_pump_total",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "DongleAtPumpTotal"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QLabel",
                  "objectName": "dongleSuctionPacketIntervalLabel",
                  "text": "包间隔：-- ms"
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "CH1 实时-- 最高-- 最低-- 有效0 漏峰0 弱峰0 最大峰-- 频率--",
                      "objectName": "dongleSuctionCh1PeakStatLabel"
                    },
                    {
                      "label": "CH2 实时-- 最高-- 最低-- 有效0 漏峰0 弱峰0 最大峰-- 频率--",
                      "objectName": "dongleSuctionCh2PeakStatLabel"
                    },
                    {
                      "label": "CH3 实时-- 最高-- 最低-- 有效0 漏峰0 弱峰0 最大峰-- 频率--",
                      "objectName": "dongleSuctionCh3PeakStatLabel"
                    }
                  ]
                },
                {
                  "type": "QCheckBox",
                  "objectName": "dongleSuctionPeakGuideLinesCheck",
                  "text": "显示峰检辅助线"
                },
                {
                  "type": "QTableWidget",
                  "objectName": "tableDongleSegments",
                  "note": "[表格: tableDongleSegments]"
                }
              ]
            },
            {
              "objectName": "tab_dongle_at",
              "title": "AT 测试",
              "content": [
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_dongle_at_connect",
                  "title": "连接模式（MAC 使用顶部 macInput）",
                  "button_rows": [],
                  "controls": [
                    {
                      "type": "QPushButton",
                      "objectName": "btnDongleAtDcon",
                      "text": "AT+DCON 直连"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "btnDongleAtOta",
                      "text": "AT+OTA 连接"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "btnDongleAtBle",
                      "text": "AT+BLE 连接"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "btnDongleAtMain",
                      "text": "AT+MAIN 连接"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "btnDongleAtDisconnect",
                      "text": "断开 BLE"
                    }
                  ],
                  "fields": [],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_dongle_at_passthrough",
                  "title": "透传与参数",
                  "button_rows": [],
                  "controls": [
                    {
                      "type": "QCheckBox",
                      "objectName": "chkDongleAtOtaData",
                      "text": "AT+OTADATA"
                    },
                    {
                      "type": "QCheckBox",
                      "objectName": "chkDongleAtMainData",
                      "text": "AT+MAINDATA"
                    },
                    {
                      "type": "QLabel",
                      "objectName": "label_dongle_at_ble_mtu",
                      "text": "BLE MTU(字节)"
                    },
                    {
                      "type": "QLineEdit",
                      "objectName": "DongleAtBleMtuSize"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "btnDongleAtSetBleMtu",
                      "text": "设置BLE MTU"
                    },
                    {
                      "type": "QLabel",
                      "objectName": "label_dongle_at_ota_pkt_size",
                      "text": "OTA切包(字节)"
                    },
                    {
                      "type": "QLineEdit",
                      "objectName": "DongleAtOtaPktSize"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "btnDongleAtSetOtaPkt",
                      "text": "设置切包"
                    }
                  ],
                  "fields": [],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_dongle_at_log",
                  "title": "日志",
                  "button_rows": [],
                  "controls": [
                    {
                      "type": "QCheckBox",
                      "objectName": "chkDongleAtBleLog",
                      "text": "AT+BLERSSILOG"
                    },
                    {
                      "type": "QCheckBox",
                      "objectName": "chkDongleAtBleDeviceLog",
                      "text": "AT+BLEDEVICELOG"
                    }
                  ],
                  "fields": [],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_dongle_at_adc",
                  "title": "ADC 采样",
                  "button_rows": [],
                  "controls": [
                    {
                      "type": "QCheckBox",
                      "objectName": "checkBox_adcSwitch",
                      "text": "AT+HSADC 高量程采样"
                    },
                    {
                      "type": "QCheckBox",
                      "objectName": "checkBox_lsAdcSwitch",
                      "text": "AT+LSADC 低量程采样"
                    }
                  ],
                  "fields": [],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_dongle_at_bomb",
                  "title": "船运炸弹 AT+BOMB",
                  "button_rows": [],
                  "controls": [
                    {
                      "type": "QPushButton",
                      "objectName": "ship_bomb",
                      "text": "船运炸弹"
                    }
                  ],
                  "fields": [
                    {
                      "label": "信号强度",
                      "objectName": "label_86",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "bombrssi"
                      }
                    },
                    {
                      "label": "设备名字",
                      "objectName": "label_85",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "bombname"
                      }
                    },
                    {
                      "label": "连接间隔",
                      "objectName": "label_87",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "bombinterval"
                      }
                    },
                    {
                      "label": "指令",
                      "objectName": "label_bomb_command",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "bombcommand"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_dongle_at_query",
                  "title": "查询 / 自定义 AT",
                  "button_rows": [],
                  "controls": [
                    {
                      "type": "QPushButton",
                      "objectName": "btnDongleAtGmac",
                      "text": "查询 GMAC"
                    },
                    {
                      "type": "QLineEdit",
                      "objectName": "lineDongleAtCustom",
                      "placeholder": "自定义 AT 行，例如 AT+BLEMTU=247（可不写末尾 CRLF）"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "btnDongleAtSendCustom",
                      "text": "发送"
                    }
                  ],
                  "fields": [],
                  "tables": [],
                  "children": []
                }
              ]
            }
          ]
        }
      },
      {
        "objectName": "tab_debug_group_ota",
        "title": "OTA/压测",
        "content": [
          {
            "type": "QTabWidget",
            "objectName": "tabWidget_ota",
            "tabs": [
              {
                "objectName": "tab_7",
                "title": "本地OTA",
                "content": [
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "文件类型：",
                        "objectName": "label_20",
                        "field": {
                          "type": "QComboBox",
                          "objectName": "version_type_1"
                        }
                      },
                      {
                        "label": "文件大小：",
                        "objectName": "label_16",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "file_size_1"
                        }
                      },
                      {
                        "label": "版本号：",
                        "objectName": "label_17",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "version_type_3"
                        }
                      },
                      {
                        "label": "路径：",
                        "objectName": "label_19",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "url_1"
                        }
                      },
                      {
                        "label": "MD5：",
                        "objectName": "label_18",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "md5_1"
                        }
                      },
                      {
                        "field": {
                          "type": "QCheckBox",
                          "objectName": "is_ota_1",
                          "text": "是否升级此列"
                        }
                      }
                    ]
                  },
                  {
                    "kind": "button_row",
                    "buttons": [
                      "开始升级"
                    ]
                  },
                  {
                    "type": "QLabel",
                    "objectName": "local_ota_result",
                    "text": "OTA"
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "文件类型：",
                        "objectName": "label_24",
                        "field": {
                          "type": "QComboBox",
                          "objectName": "version_type_2"
                        }
                      },
                      {
                        "label": "文件大小：",
                        "objectName": "label_21",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "file_size_2"
                        }
                      },
                      {
                        "label": "版本号：",
                        "objectName": "label_22",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "version_type_4"
                        }
                      },
                      {
                        "label": "路径：",
                        "objectName": "label_23",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "url_2"
                        }
                      },
                      {
                        "label": "MD5：",
                        "objectName": "label_25",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "md5_2"
                        }
                      },
                      {
                        "field": {
                          "type": "QCheckBox",
                          "objectName": "is_ota_2",
                          "text": "是否升级此列"
                        }
                      }
                    ]
                  }
                ]
              },
              {
                "objectName": "tab_8",
                "title": "云端OTA",
                "content": [
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "<html><head/><body><p align=\"center\">ota测试路径</p></body></html>",
                        "objectName": "label_102",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "iot_url_test"
                        }
                      }
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "<html><head/><body><p align=\"center\">北美测试路径</p></body></html>",
                        "objectName": "label_104",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "iot_url_america"
                        }
                      }
                    ]
                  },
                  {
                    "type": "QLabel",
                    "objectName": "wifiota_fai_times",
                    "text": "失败升级次数：0"
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "测试环境密码：",
                        "objectName": "label_28",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "ota_secret"
                        }
                      },
                      {
                        "label": "输入MAC连接：",
                        "objectName": "label_26",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "wifiOtaMacInput",
                          "placeholder": "切换英文输入再扫码"
                        }
                      },
                      {
                        "field": {
                          "type": "QCheckBox",
                          "objectName": "is_start_ota",
                          "text": "扫SN查表后自动WiFi OTA"
                        }
                      }
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "耗时(s)：",
                        "objectName": "label_27"
                      }
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "<html><head/><body><p align=\"center\">ota生产路径</p></body></html>",
                        "objectName": "label_103",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "iot_url_product"
                        }
                      }
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "<html><head/><body><p align=\"center\">北美生产路径</p></body></html>",
                        "objectName": "label_105",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "iot_url_america_product"
                        }
                      }
                    ]
                  },
                  {
                    "type": "QLabel",
                    "objectName": "wifiota_suc_times",
                    "text": "成功升级次数：0"
                  },
                  {
                    "type": "QPlainTextEdit",
                    "objectName": "testMsg"
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "field": {
                          "type": "QRadioButton",
                          "objectName": "csv_ver",
                          "text": "本地证书"
                        }
                      },
                      {
                        "field": {
                          "type": "QRadioButton",
                          "objectName": "test_ver",
                          "text": "测试环境"
                        }
                      },
                      {
                        "field": {
                          "type": "QRadioButton",
                          "objectName": "product_ver",
                          "text": "生产环境"
                        }
                      },
                      {
                        "field": {
                          "type": "QRadioButton",
                          "objectName": "america_ver",
                          "text": "北美测试（加密）"
                        }
                      },
                      {
                        "field": {
                          "type": "QRadioButton",
                          "objectName": "america_product_ver",
                          "text": "北美生产（加密）"
                        }
                      }
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "Wi-Fi 账号",
                        "objectName": "label_89",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "wifiUserName_2"
                        }
                      },
                      {
                        "label": "Wi-Fi 密码",
                        "objectName": "label_90",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "wifiPassword_2"
                        }
                      }
                    ]
                  },
                  {
                    "type": "QCheckBox",
                    "objectName": "is_wifiota_press",
                    "text": "OTA压测"
                  },
                  {
                    "type": "QCheckBox",
                    "objectName": "cloud_key",
                    "text": "是否启用一机一密"
                  }
                ]
              },
              {
                "objectName": "tab_16",
                "title": "蓝牙ota",
                "content": [
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "输入MAC连接：",
                        "objectName": "label_29",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "bleotamacInput"
                        }
                      },
                      {
                        "field": {
                          "type": "QCheckBox",
                          "objectName": "is_start_bleota",
                          "text": "扫SN查表后自动蓝牙OTA"
                        }
                      },
                      {
                        "field": {
                          "type": "QCheckBox",
                          "objectName": "is_bleota_press",
                          "text": "蓝牙OTA压测"
                        }
                      }
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "ota发送数据包时间间隔（ms）",
                        "objectName": "label_92",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "OtaTimeInterval"
                        }
                      },
                      {
                        "label": "块忙等待时间（ms）",
                        "objectName": "label_ota_block_busy_wait",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "OtaBlockBusyWaitMs"
                        }
                      },
                      {
                        "label": "分片大小（字节）",
                        "objectName": "label_ota_fragment_size",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "OtaFragmentSize"
                        }
                      },
                      {
                        "field": {
                          "type": "QCheckBox",
                          "objectName": "is_img_res_ota_v2",
                          "text": "图片资源OTA v2"
                        }
                      },
                      {
                        "field": {
                          "type": "QCheckBox",
                          "objectName": "is_clear_mac",
                          "text": "是否升级完成清空mac"
                        }
                      }
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "固件发送进度",
                        "objectName": "label_83"
                      }
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "耗时",
                        "objectName": "label_88"
                      }
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "整体接收进度",
                        "objectName": "label_84"
                      }
                    ]
                  },
                  {
                    "type": "QLabel",
                    "objectName": "bleotaresult",
                    "text": "<html><head/><body><p align=\"center\">WAIT</p></body></html>"
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "otaFilePath_source"
                        }
                      }
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "资源发送进度",
                        "objectName": "label_106"
                      }
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "otaFilePath"
                        }
                      }
                    ]
                  },
                  {
                    "type": "QPlainTextEdit",
                    "objectName": "bleOtaMsg"
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "图片资源版本(0x0002)",
                        "objectName": "label_ota_img_res_version",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "OtaImgResVersion"
                        }
                      },
                      {
                        "label": "BLE MTU(字节)",
                        "objectName": "label_ble_mtu",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "BleMtuSize"
                        }
                      },
                      {
                        "label": "OTA切包(字节)",
                        "objectName": "label_ota_pkt_size",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "OtaPktSize"
                        }
                      }
                    ]
                  }
                ]
              },
              {
                "objectName": "tab_2",
                "title": "老化测试",
                "content": [
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_4",
                    "title": "老化测试",
                    "button_rows": [
                      [
                        "进入老化",
                        "退出老化",
                        "读取老化状态",
                        "工厂老化1开关"
                      ]
                    ],
                    "controls": [],
                    "fields": [
                      {
                        "label": "老化模式",
                        "objectName": "label",
                        "field": {
                          "type": "QComboBox",
                          "objectName": "burningModeCombo",
                          "items": [
                            "老化1",
                            "老化2",
                            "老化3",
                            "老化4"
                          ]
                        }
                      },
                      {
                        "label": "老化时间(秒)",
                        "objectName": "label_108",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "burningModetime"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  }
                ]
              },
              {
                "objectName": "tab_9",
                "title": "压力测试",
                "content": [
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "field": {
                          "type": "QRadioButton",
                          "objectName": "radioButton",
                          "text": "RadioButton"
                        }
                      }
                    ]
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_13",
                    "title": "连接蓝牙",
                    "button_rows": [],
                    "controls": [
                      {
                        "type": "QPushButton",
                        "objectName": "getInfoPushButton",
                        "text": "获取信息"
                      }
                    ],
                    "fields": [],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_14",
                    "title": "配网信息",
                    "button_rows": [],
                    "controls": [
                      {
                        "type": "QPushButton",
                        "objectName": "configWifiPushButton_2",
                        "text": "配网"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "otaTestPushButton_2",
                        "text": "单次OTA"
                      }
                    ],
                    "fields": [
                      {
                        "label": "产品Key",
                        "objectName": "label_30",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "productKey"
                        }
                      },
                      {
                        "label": "设备名",
                        "objectName": "label_31",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "deviceName"
                        }
                      },
                      {
                        "label": "设备秘钥",
                        "objectName": "label_32",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "deviceSecret"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_10",
                    "title": "压测",
                    "button_rows": [],
                    "controls": [
                      {
                        "type": "QPushButton",
                        "objectName": "bleTestPushButton",
                        "text": "蓝牙压测"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "configWifiPushButton",
                        "text": "配网压测"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "otaTestPushButton",
                        "text": "固件OTA压测"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "stopTestPushButton",
                        "text": "结束以上测试"
                      }
                    ],
                    "fields": [
                      {
                        "label": "测试间隔(s)",
                        "objectName": "label_33",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "testPeriodSpin"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  }
                ]
              }
            ]
          }
        ],
        "tabWidget_ota": {
          "type": "QTabWidget",
          "objectName": "tabWidget_ota",
          "tabs": [
            {
              "objectName": "tab_7",
              "title": "本地OTA",
              "content": [
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "文件类型：",
                      "objectName": "label_20",
                      "field": {
                        "type": "QComboBox",
                        "objectName": "version_type_1"
                      }
                    },
                    {
                      "label": "文件大小：",
                      "objectName": "label_16",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "file_size_1"
                      }
                    },
                    {
                      "label": "版本号：",
                      "objectName": "label_17",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "version_type_3"
                      }
                    },
                    {
                      "label": "路径：",
                      "objectName": "label_19",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "url_1"
                      }
                    },
                    {
                      "label": "MD5：",
                      "objectName": "label_18",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "md5_1"
                      }
                    },
                    {
                      "field": {
                        "type": "QCheckBox",
                        "objectName": "is_ota_1",
                        "text": "是否升级此列"
                      }
                    }
                  ]
                },
                {
                  "kind": "button_row",
                  "buttons": [
                    "开始升级"
                  ]
                },
                {
                  "type": "QLabel",
                  "objectName": "local_ota_result",
                  "text": "OTA"
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "文件类型：",
                      "objectName": "label_24",
                      "field": {
                        "type": "QComboBox",
                        "objectName": "version_type_2"
                      }
                    },
                    {
                      "label": "文件大小：",
                      "objectName": "label_21",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "file_size_2"
                      }
                    },
                    {
                      "label": "版本号：",
                      "objectName": "label_22",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "version_type_4"
                      }
                    },
                    {
                      "label": "路径：",
                      "objectName": "label_23",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "url_2"
                      }
                    },
                    {
                      "label": "MD5：",
                      "objectName": "label_25",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "md5_2"
                      }
                    },
                    {
                      "field": {
                        "type": "QCheckBox",
                        "objectName": "is_ota_2",
                        "text": "是否升级此列"
                      }
                    }
                  ]
                }
              ]
            },
            {
              "objectName": "tab_8",
              "title": "云端OTA",
              "content": [
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "<html><head/><body><p align=\"center\">ota测试路径</p></body></html>",
                      "objectName": "label_102",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "iot_url_test"
                      }
                    }
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "<html><head/><body><p align=\"center\">北美测试路径</p></body></html>",
                      "objectName": "label_104",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "iot_url_america"
                      }
                    }
                  ]
                },
                {
                  "type": "QLabel",
                  "objectName": "wifiota_fai_times",
                  "text": "失败升级次数：0"
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "测试环境密码：",
                      "objectName": "label_28",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "ota_secret"
                      }
                    },
                    {
                      "label": "输入MAC连接：",
                      "objectName": "label_26",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "wifiOtaMacInput",
                        "placeholder": "切换英文输入再扫码"
                      }
                    },
                    {
                      "field": {
                        "type": "QCheckBox",
                        "objectName": "is_start_ota",
                        "text": "扫SN查表后自动WiFi OTA"
                      }
                    }
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "耗时(s)：",
                      "objectName": "label_27"
                    }
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "<html><head/><body><p align=\"center\">ota生产路径</p></body></html>",
                      "objectName": "label_103",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "iot_url_product"
                      }
                    }
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "<html><head/><body><p align=\"center\">北美生产路径</p></body></html>",
                      "objectName": "label_105",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "iot_url_america_product"
                      }
                    }
                  ]
                },
                {
                  "type": "QLabel",
                  "objectName": "wifiota_suc_times",
                  "text": "成功升级次数：0"
                },
                {
                  "type": "QPlainTextEdit",
                  "objectName": "testMsg"
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "field": {
                        "type": "QRadioButton",
                        "objectName": "csv_ver",
                        "text": "本地证书"
                      }
                    },
                    {
                      "field": {
                        "type": "QRadioButton",
                        "objectName": "test_ver",
                        "text": "测试环境"
                      }
                    },
                    {
                      "field": {
                        "type": "QRadioButton",
                        "objectName": "product_ver",
                        "text": "生产环境"
                      }
                    },
                    {
                      "field": {
                        "type": "QRadioButton",
                        "objectName": "america_ver",
                        "text": "北美测试（加密）"
                      }
                    },
                    {
                      "field": {
                        "type": "QRadioButton",
                        "objectName": "america_product_ver",
                        "text": "北美生产（加密）"
                      }
                    }
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "Wi-Fi 账号",
                      "objectName": "label_89",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "wifiUserName_2"
                      }
                    },
                    {
                      "label": "Wi-Fi 密码",
                      "objectName": "label_90",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "wifiPassword_2"
                      }
                    }
                  ]
                },
                {
                  "type": "QCheckBox",
                  "objectName": "is_wifiota_press",
                  "text": "OTA压测"
                },
                {
                  "type": "QCheckBox",
                  "objectName": "cloud_key",
                  "text": "是否启用一机一密"
                }
              ]
            },
            {
              "objectName": "tab_16",
              "title": "蓝牙ota",
              "content": [
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "输入MAC连接：",
                      "objectName": "label_29",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "bleotamacInput"
                      }
                    },
                    {
                      "field": {
                        "type": "QCheckBox",
                        "objectName": "is_start_bleota",
                        "text": "扫SN查表后自动蓝牙OTA"
                      }
                    },
                    {
                      "field": {
                        "type": "QCheckBox",
                        "objectName": "is_bleota_press",
                        "text": "蓝牙OTA压测"
                      }
                    }
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "ota发送数据包时间间隔（ms）",
                      "objectName": "label_92",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "OtaTimeInterval"
                      }
                    },
                    {
                      "label": "块忙等待时间（ms）",
                      "objectName": "label_ota_block_busy_wait",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "OtaBlockBusyWaitMs"
                      }
                    },
                    {
                      "label": "分片大小（字节）",
                      "objectName": "label_ota_fragment_size",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "OtaFragmentSize"
                      }
                    },
                    {
                      "field": {
                        "type": "QCheckBox",
                        "objectName": "is_img_res_ota_v2",
                        "text": "图片资源OTA v2"
                      }
                    },
                    {
                      "field": {
                        "type": "QCheckBox",
                        "objectName": "is_clear_mac",
                        "text": "是否升级完成清空mac"
                      }
                    }
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "固件发送进度",
                      "objectName": "label_83"
                    }
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "耗时",
                      "objectName": "label_88"
                    }
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "整体接收进度",
                      "objectName": "label_84"
                    }
                  ]
                },
                {
                  "type": "QLabel",
                  "objectName": "bleotaresult",
                  "text": "<html><head/><body><p align=\"center\">WAIT</p></body></html>"
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "otaFilePath_source"
                      }
                    }
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "资源发送进度",
                      "objectName": "label_106"
                    }
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "otaFilePath"
                      }
                    }
                  ]
                },
                {
                  "type": "QPlainTextEdit",
                  "objectName": "bleOtaMsg"
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "图片资源版本(0x0002)",
                      "objectName": "label_ota_img_res_version",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "OtaImgResVersion"
                      }
                    },
                    {
                      "label": "BLE MTU(字节)",
                      "objectName": "label_ble_mtu",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "BleMtuSize"
                      }
                    },
                    {
                      "label": "OTA切包(字节)",
                      "objectName": "label_ota_pkt_size",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "OtaPktSize"
                      }
                    }
                  ]
                }
              ]
            },
            {
              "objectName": "tab_2",
              "title": "老化测试",
              "content": [
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_4",
                  "title": "老化测试",
                  "button_rows": [
                    [
                      "进入老化",
                      "退出老化",
                      "读取老化状态",
                      "工厂老化1开关"
                    ]
                  ],
                  "controls": [],
                  "fields": [
                    {
                      "label": "老化模式",
                      "objectName": "label",
                      "field": {
                        "type": "QComboBox",
                        "objectName": "burningModeCombo",
                        "items": [
                          "老化1",
                          "老化2",
                          "老化3",
                          "老化4"
                        ]
                      }
                    },
                    {
                      "label": "老化时间(秒)",
                      "objectName": "label_108",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "burningModetime"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                }
              ]
            },
            {
              "objectName": "tab_9",
              "title": "压力测试",
              "content": [
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "field": {
                        "type": "QRadioButton",
                        "objectName": "radioButton",
                        "text": "RadioButton"
                      }
                    }
                  ]
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_13",
                  "title": "连接蓝牙",
                  "button_rows": [],
                  "controls": [
                    {
                      "type": "QPushButton",
                      "objectName": "getInfoPushButton",
                      "text": "获取信息"
                    }
                  ],
                  "fields": [],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_14",
                  "title": "配网信息",
                  "button_rows": [],
                  "controls": [
                    {
                      "type": "QPushButton",
                      "objectName": "configWifiPushButton_2",
                      "text": "配网"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "otaTestPushButton_2",
                      "text": "单次OTA"
                    }
                  ],
                  "fields": [
                    {
                      "label": "产品Key",
                      "objectName": "label_30",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "productKey"
                      }
                    },
                    {
                      "label": "设备名",
                      "objectName": "label_31",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "deviceName"
                      }
                    },
                    {
                      "label": "设备秘钥",
                      "objectName": "label_32",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "deviceSecret"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_10",
                  "title": "压测",
                  "button_rows": [],
                  "controls": [
                    {
                      "type": "QPushButton",
                      "objectName": "bleTestPushButton",
                      "text": "蓝牙压测"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "configWifiPushButton",
                      "text": "配网压测"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "otaTestPushButton",
                      "text": "固件OTA压测"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "stopTestPushButton",
                      "text": "结束以上测试"
                    }
                  ],
                  "fields": [
                    {
                      "label": "测试间隔(s)",
                      "objectName": "label_33",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "testPeriodSpin"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                }
              ]
            }
          ]
        }
      },
      {
        "objectName": "tab_debug_group_misc",
        "title": "其它",
        "content": [
          {
            "type": "QTabWidget",
            "objectName": "tabWidget_debug_misc",
            "tabs": [
              {
                "objectName": "tab_misc_special",
                "title": "专用调试",
                "content": [
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_qaiot_cycle",
                    "title": "Qaiot 循环上报",
                    "button_rows": [],
                    "controls": [],
                    "fields": [
                      {
                        "label": "循环上报",
                        "objectName": "label_qaiot_cycle",
                        "field": {
                          "type": "QComboBox",
                          "objectName": "qaiot_cycle_type"
                        }
                      },
                      {
                        "label": "间隔ms",
                        "objectName": "label_qaiot_cycle_interval",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_cycle_interval"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "groupBox_qaiot_ex",
                    "title": "Qaiot 异常阈值",
                    "button_rows": [],
                    "controls": [],
                    "fields": [
                      {
                        "label": "异常阈值",
                        "objectName": "label_qaiot_ex",
                        "field": {
                          "type": "QComboBox",
                          "objectName": "qaiot_ex_type"
                        }
                      },
                      {
                        "label": "value",
                        "objectName": "label_qaiot_ex_value",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_ex_value"
                        }
                      },
                      {
                        "label": "low",
                        "objectName": "label_qaiot_ex_low",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_ex_low"
                        }
                      },
                      {
                        "label": "high",
                        "objectName": "label_qaiot_ex_high",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "qaiot_ex_high"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  },
                  {
                    "type": "QGroupBox",
                    "objectName": "verticalGroupBox_3",
                    "title": "音乐测试",
                    "button_rows": [],
                    "controls": [
                      {
                        "type": "QComboBox",
                        "objectName": "music_combo"
                      }
                    ],
                    "fields": [
                      {
                        "label": "录音时间",
                        "objectName": "label_91",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "music_time"
                        }
                      }
                    ],
                    "tables": [],
                    "children": []
                  },
                  {
                    "kind": "button_row",
                    "buttons": [
                      "设备黑盒获取",
                      "获取设备状态"
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "Service ID",
                        "objectName": "label_custom_service",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "custom_service_id"
                        }
                      },
                      {
                        "label": "TLV Type",
                        "objectName": "label_custom_tlv",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "custom_tlv_type"
                        }
                      },
                      {
                        "label": "TLV Value",
                        "objectName": "label_custom_value",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "custom_value_hex",
                          "placeholder": "01 02 0A FF"
                        }
                      },
                      {
                        "label": "action",
                        "objectName": "label_custom_action",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "custom_action_name",
                          "value": "上层自定义消息"
                        }
                      }
                    ]
                  }
                ]
              },
              {
                "objectName": "tab_12",
                "title": "ui预览",
                "content": [
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "传输单个音频文件",
                        "objectName": "high_speed_tp"
                      },
                      {
                        "label": "批量处理音频文件夹",
                        "objectName": "active_picture"
                      }
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "Wi-Fi 账号",
                        "objectName": "label_96",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "uissidInput"
                        }
                      },
                      {
                        "label": "Wi-Fi 密码",
                        "objectName": "label_97",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "uipasswordInput"
                        }
                      }
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "增加音量",
                        "objectName": "label_93"
                      },
                      {
                        "label": "0",
                        "objectName": "label_94"
                      }
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "播放速度",
                        "objectName": "label_95",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "play_speed",
                          "placeholder": "输入播放间隔时间"
                        }
                      },
                      {
                        "label": "ms/张",
                        "objectName": "label_98"
                      }
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "field": {
                          "type": "QCheckBox",
                          "objectName": "is_audio_mode",
                          "text": "是否是音频传输模式"
                        }
                      },
                      {
                        "field": {
                          "type": "QCheckBox",
                          "objectName": "checkBox_uiPreviewRgb888",
                          "text": "RGB888发送（24位真彩）"
                        }
                      },
                      {
                        "field": {
                          "type": "QCheckBox",
                          "objectName": "checkBox_uiPreviewJpegLoss",
                          "text": "模拟JPG压缩画质"
                        }
                      },
                      {
                        "label": "JPG质量",
                        "objectName": "label_uiPreviewJpegQuality",
                        "field": {
                          "type": "QSpinBox",
                          "objectName": "spinBox_uiPreviewJpegQuality"
                        }
                      },
                      {
                        "label": "JPG采样",
                        "objectName": "label_uiPreviewJpegSubsampling",
                        "field": {
                          "type": "QComboBox",
                          "objectName": "comboBox_uiPreviewJpegSubsampling",
                          "items": [
                            "4:4:4",
                            "4:2:2"
                          ]
                        }
                      }
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "Y轴坐标",
                        "objectName": "label_99",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "ui_ypos"
                        }
                      },
                      {
                        "label": "像素点",
                        "objectName": "label_100"
                      }
                    ]
                  },
                  {
                    "type": "QPushButton",
                    "objectName": "play_picture",
                    "text": "播放"
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "label": "设备的ip地址：",
                        "objectName": "label_42",
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "ui_ip"
                        }
                      }
                    ]
                  },
                  {
                    "type": "QPushButton",
                    "objectName": "get_battery_level",
                    "text": "获取电量"
                  },
                  {
                    "type": "QPushButton",
                    "objectName": "up_picture",
                    "text": "上一张"
                  },
                  {
                    "type": "QPushButton",
                    "objectName": "down_picture",
                    "text": "下一张"
                  },
                  {
                    "type": "QPushButton",
                    "objectName": "clear_picture",
                    "text": "清空设备图片"
                  },
                  {
                    "type": "QPushButton",
                    "objectName": "init_ui_data",
                    "text": "格式化全部"
                  }
                ]
              },
              {
                "objectName": "tab_15",
                "title": "NFC",
                "content": [
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "field": {
                          "type": "QComboBox",
                          "objectName": "nfc_combo"
                        }
                      },
                      {
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "nfc_sn"
                        }
                      },
                      {
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "nfc_mac"
                        }
                      }
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "field": {
                          "type": "QLineEdit",
                          "objectName": "nfc_name"
                        }
                      }
                    ]
                  },
                  {
                    "kind": "form_row",
                    "fields": [
                      {
                        "field": {
                          "type": "QComboBox",
                          "objectName": "NfcComboBox"
                        }
                      }
                    ]
                  }
                ]
              },
              {
                "objectName": "tab_14",
                "title": "测试页面",
                "content": [
                  {
                    "type": "QLineEdit",
                    "objectName": "calculate",
                    "placeholder": "将六轴数据生成校准结果敲回车接口"
                  },
                  {
                    "type": "QPushButton",
                    "objectName": "delefile",
                    "text": "删除文件"
                  },
                  {
                    "type": "QPushButton",
                    "objectName": "get_noisy",
                    "text": "开始采集噪声"
                  },
                  {
                    "type": "QPushButton",
                    "objectName": "stop_noisy",
                    "text": "停止采集噪声"
                  },
                  {
                    "type": "QWidget",
                    "objectName": "layoutWidget",
                    "content": [
                      {
                        "type": "QPushButton",
                        "objectName": "set_hw_ver",
                        "text": "设置硬件版本号"
                      },
                      {
                        "type": "QLineEdit",
                        "objectName": "hw_ver"
                      }
                    ]
                  },
                  {
                    "type": "QWidget",
                    "objectName": "horizontalLayoutWidget",
                    "content": [
                      {
                        "type": "QLabel",
                        "objectName": "label_101",
                        "text": "AI指令："
                      },
                      {
                        "type": "QLineEdit",
                        "objectName": "AITestLine",
                        "placeholder": "输入AI处理需求，也可长按语音输入"
                      },
                      {
                        "type": "QPushButton",
                        "objectName": "speakAi",
                        "text": "长按语音"
                      }
                    ]
                  },
                  {
                    "type": "QPushButton",
                    "objectName": "pushButton_3",
                    "text": "指令测试"
                  },
                  {
                    "type": "QWidget",
                    "objectName": "layoutWidget_2",
                    "content": [
                      {
                        "kind": "button_row",
                        "buttons": [
                          "开始录音",
                          "结束录音"
                        ]
                      },
                      {
                        "kind": "button_row",
                        "buttons": [
                          "开始上传录音文件",
                          "停止上传录音文件"
                        ]
                      },
                      {
                        "kind": "button_row",
                        "buttons": [
                          "获取SD卡状态",
                          "获取光敏电阻信息"
                        ]
                      }
                    ]
                  }
                ]
              }
            ]
          }
        ],
        "tabWidget_debug_misc": {
          "type": "QTabWidget",
          "objectName": "tabWidget_debug_misc",
          "tabs": [
            {
              "objectName": "tab_misc_special",
              "title": "专用调试",
              "content": [
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_qaiot_cycle",
                  "title": "Qaiot 循环上报",
                  "button_rows": [],
                  "controls": [],
                  "fields": [
                    {
                      "label": "循环上报",
                      "objectName": "label_qaiot_cycle",
                      "field": {
                        "type": "QComboBox",
                        "objectName": "qaiot_cycle_type"
                      }
                    },
                    {
                      "label": "间隔ms",
                      "objectName": "label_qaiot_cycle_interval",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_cycle_interval"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QGroupBox",
                  "objectName": "groupBox_qaiot_ex",
                  "title": "Qaiot 异常阈值",
                  "button_rows": [],
                  "controls": [],
                  "fields": [
                    {
                      "label": "异常阈值",
                      "objectName": "label_qaiot_ex",
                      "field": {
                        "type": "QComboBox",
                        "objectName": "qaiot_ex_type"
                      }
                    },
                    {
                      "label": "value",
                      "objectName": "label_qaiot_ex_value",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_ex_value"
                      }
                    },
                    {
                      "label": "low",
                      "objectName": "label_qaiot_ex_low",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_ex_low"
                      }
                    },
                    {
                      "label": "high",
                      "objectName": "label_qaiot_ex_high",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "qaiot_ex_high"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                },
                {
                  "type": "QGroupBox",
                  "objectName": "verticalGroupBox_3",
                  "title": "音乐测试",
                  "button_rows": [],
                  "controls": [
                    {
                      "type": "QComboBox",
                      "objectName": "music_combo"
                    }
                  ],
                  "fields": [
                    {
                      "label": "录音时间",
                      "objectName": "label_91",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "music_time"
                      }
                    }
                  ],
                  "tables": [],
                  "children": []
                },
                {
                  "kind": "button_row",
                  "buttons": [
                    "设备黑盒获取",
                    "获取设备状态"
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "Service ID",
                      "objectName": "label_custom_service",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "custom_service_id"
                      }
                    },
                    {
                      "label": "TLV Type",
                      "objectName": "label_custom_tlv",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "custom_tlv_type"
                      }
                    },
                    {
                      "label": "TLV Value",
                      "objectName": "label_custom_value",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "custom_value_hex",
                        "placeholder": "01 02 0A FF"
                      }
                    },
                    {
                      "label": "action",
                      "objectName": "label_custom_action",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "custom_action_name",
                        "value": "上层自定义消息"
                      }
                    }
                  ]
                }
              ]
            },
            {
              "objectName": "tab_12",
              "title": "ui预览",
              "content": [
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "传输单个音频文件",
                      "objectName": "high_speed_tp"
                    },
                    {
                      "label": "批量处理音频文件夹",
                      "objectName": "active_picture"
                    }
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "Wi-Fi 账号",
                      "objectName": "label_96",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "uissidInput"
                      }
                    },
                    {
                      "label": "Wi-Fi 密码",
                      "objectName": "label_97",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "uipasswordInput"
                      }
                    }
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "增加音量",
                      "objectName": "label_93"
                    },
                    {
                      "label": "0",
                      "objectName": "label_94"
                    }
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "播放速度",
                      "objectName": "label_95",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "play_speed",
                        "placeholder": "输入播放间隔时间"
                      }
                    },
                    {
                      "label": "ms/张",
                      "objectName": "label_98"
                    }
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "field": {
                        "type": "QCheckBox",
                        "objectName": "is_audio_mode",
                        "text": "是否是音频传输模式"
                      }
                    },
                    {
                      "field": {
                        "type": "QCheckBox",
                        "objectName": "checkBox_uiPreviewRgb888",
                        "text": "RGB888发送（24位真彩）"
                      }
                    },
                    {
                      "field": {
                        "type": "QCheckBox",
                        "objectName": "checkBox_uiPreviewJpegLoss",
                        "text": "模拟JPG压缩画质"
                      }
                    },
                    {
                      "label": "JPG质量",
                      "objectName": "label_uiPreviewJpegQuality",
                      "field": {
                        "type": "QSpinBox",
                        "objectName": "spinBox_uiPreviewJpegQuality"
                      }
                    },
                    {
                      "label": "JPG采样",
                      "objectName": "label_uiPreviewJpegSubsampling",
                      "field": {
                        "type": "QComboBox",
                        "objectName": "comboBox_uiPreviewJpegSubsampling",
                        "items": [
                          "4:4:4",
                          "4:2:2"
                        ]
                      }
                    }
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "Y轴坐标",
                      "objectName": "label_99",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "ui_ypos"
                      }
                    },
                    {
                      "label": "像素点",
                      "objectName": "label_100"
                    }
                  ]
                },
                {
                  "type": "QPushButton",
                  "objectName": "play_picture",
                  "text": "播放"
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "label": "设备的ip地址：",
                      "objectName": "label_42",
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "ui_ip"
                      }
                    }
                  ]
                },
                {
                  "type": "QPushButton",
                  "objectName": "get_battery_level",
                  "text": "获取电量"
                },
                {
                  "type": "QPushButton",
                  "objectName": "up_picture",
                  "text": "上一张"
                },
                {
                  "type": "QPushButton",
                  "objectName": "down_picture",
                  "text": "下一张"
                },
                {
                  "type": "QPushButton",
                  "objectName": "clear_picture",
                  "text": "清空设备图片"
                },
                {
                  "type": "QPushButton",
                  "objectName": "init_ui_data",
                  "text": "格式化全部"
                }
              ]
            },
            {
              "objectName": "tab_15",
              "title": "NFC",
              "content": [
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "field": {
                        "type": "QComboBox",
                        "objectName": "nfc_combo"
                      }
                    },
                    {
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "nfc_sn"
                      }
                    },
                    {
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "nfc_mac"
                      }
                    }
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "field": {
                        "type": "QLineEdit",
                        "objectName": "nfc_name"
                      }
                    }
                  ]
                },
                {
                  "kind": "form_row",
                  "fields": [
                    {
                      "field": {
                        "type": "QComboBox",
                        "objectName": "NfcComboBox"
                      }
                    }
                  ]
                }
              ]
            },
            {
              "objectName": "tab_14",
              "title": "测试页面",
              "content": [
                {
                  "type": "QLineEdit",
                  "objectName": "calculate",
                  "placeholder": "将六轴数据生成校准结果敲回车接口"
                },
                {
                  "type": "QPushButton",
                  "objectName": "delefile",
                  "text": "删除文件"
                },
                {
                  "type": "QPushButton",
                  "objectName": "get_noisy",
                  "text": "开始采集噪声"
                },
                {
                  "type": "QPushButton",
                  "objectName": "stop_noisy",
                  "text": "停止采集噪声"
                },
                {
                  "type": "QWidget",
                  "objectName": "layoutWidget",
                  "content": [
                    {
                      "type": "QPushButton",
                      "objectName": "set_hw_ver",
                      "text": "设置硬件版本号"
                    },
                    {
                      "type": "QLineEdit",
                      "objectName": "hw_ver"
                    }
                  ]
                },
                {
                  "type": "QWidget",
                  "objectName": "horizontalLayoutWidget",
                  "content": [
                    {
                      "type": "QLabel",
                      "objectName": "label_101",
                      "text": "AI指令："
                    },
                    {
                      "type": "QLineEdit",
                      "objectName": "AITestLine",
                      "placeholder": "输入AI处理需求，也可长按语音输入"
                    },
                    {
                      "type": "QPushButton",
                      "objectName": "speakAi",
                      "text": "长按语音"
                    }
                  ]
                },
                {
                  "type": "QPushButton",
                  "objectName": "pushButton_3",
                  "text": "指令测试"
                },
                {
                  "type": "QWidget",
                  "objectName": "layoutWidget_2",
                  "content": [
                    {
                      "kind": "button_row",
                      "buttons": [
                        "开始录音",
                        "结束录音"
                      ]
                    },
                    {
                      "kind": "button_row",
                      "buttons": [
                        "开始上传录音文件",
                        "停止上传录音文件"
                      ]
                    },
                    {
                      "kind": "button_row",
                      "buttons": [
                        "获取SD卡状态",
                        "获取光敏电阻信息"
                      ]
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
};
