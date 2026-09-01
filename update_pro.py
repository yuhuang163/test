import re

with open('e:/C/test/new_production.pro', 'r', encoding='utf-8') as f:
    text = f.read()

# Add INCLUDEPATH
text = text.replace('INCLUDEPATH += business/plc_v3_fixture', 'INCLUDEPATH += business/plc_v3_fixture\nINCLUDEPATH += business/hikvision_scanner')

# Add to SOURCES
text = text.replace('    business/plc_v3_fixture/plc_v3_fixture.cpp \\', '    business/plc_v3_fixture/plc_v3_fixture.cpp \\\n    business/hikvision_scanner/hikvision_scanner.cpp \\')

# Add to HEADERS
text = text.replace('    business/plc_v3_fixture/plc_v3_fixture.h \\', '    business/plc_v3_fixture/plc_v3_fixture.h \\\n    business/hikvision_scanner/hikvision_scanner.h \\')

with open('e:/C/test/new_production.pro', 'w', encoding='utf-8') as f:
    f.write(text)
