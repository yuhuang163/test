# 编译正确性与类型核查规范 (Compilation & Type Verification Standard)

## 核心原则

由于当前运行环境未安装完整的 Qt/MSVC 本地编译器套件，无法在终端直接执行本地 `jom/nmake` 自动化构建，**所有代码修改必须进行严格的人工静态核查**，杜绝低级编译错误：

1. **查阅头文件优先**：
   - 访问任何结构体（如 `TestCaseMeta`、`TestCaseDefinition`、`MesPacketData` 等）的字段前，**必须先 `view_file` 查看其原始头文件声明**；
   - 严禁凭经验或推测猜测成员变量名（例如严禁臆测 `meta.stepId` 等不存在字段）。

2. **函数签名与作用域一致性**：
   - 在 `.h` 中声明的方法，其参数类型、`const` 修饰符必须与 `.cpp` 中的实现完全一一对应；
   - 信号与槽的连接、继承关系、重载必须与 Qt 5.15 的 API 严格匹配。

3. **依赖与宏保护**：
   - 使用 Qt 控件时（如 `QLabel`、`QTableWidget`、`QTimer` 等），确认头文件已包含；
   - 字符串字面量统一使用 `QStringLiteral` 或 `QLatin1String`。
