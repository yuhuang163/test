import re

file_path = 'e:/C/test/platform/test_case/hooks/qfreework_case_hooks.cpp'
with open(file_path, 'r', encoding='utf-8') as f:
    text = f.read()

# Register the hook inside dispatch
dispatch_insertion = '''    if (hookId == QStringLiteral("HIKVISION_SCANNER_READ")) {
        fw->runHikvisionScannerReadStep();
        return;
    }
'''
if 'QStringLiteral("HIKVISION_SCANNER_READ")' not in text:
    text = text.replace('    if (hookId == QStringLiteral("JIG_CURRENT_READ")) {', dispatch_insertion + '    if (hookId == QStringLiteral("JIG_CURRENT_READ")) {')

# Register the hook inside registerAll
registerAll_insertion = '    registerDispatchHook(QStringLiteral("HIKVISION_SCANNER_READ"));\n'
if 'registerDispatchHook(QStringLiteral("HIKVISION_SCANNER_READ"))' not in text:
    text = text.replace('    registerDispatchHook(QStringLiteral("JIG_CURRENT_READ"));', registerAll_insertion + '    registerDispatchHook(QStringLiteral("JIG_CURRENT_READ"));')

with open(file_path, 'w', encoding='utf-8') as f:
    f.write(text)
