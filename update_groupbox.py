import re

with open('e:/C/test/mainwindow.ui', 'r', encoding='utf-8') as f:
    text = f.read()

def replace_groupbox_policy(match):
    block = match.group(0)
    return block.replace('vsizetype="Fixed"', 'vsizetype="Expanding"')

# Replace sizePolicy specifically for groupBox_refImage
text = re.sub(
    r'<widget class="QGroupBox" name="groupBox_refImage">.*?<property name="sizePolicy">.*?<sizepolicy hsizetype="Preferred" vsizetype="Fixed">',
    replace_groupbox_policy,
    text,
    flags=re.DOTALL
)

# Replace sizePolicy specifically for groupBox_currImage
text = re.sub(
    r'<widget class="QGroupBox" name="groupBox_currImage">.*?<property name="sizePolicy">.*?<sizepolicy hsizetype="Preferred" vsizetype="Fixed">',
    replace_groupbox_policy,
    text,
    flags=re.DOTALL
)

with open('e:/C/test/mainwindow.ui', 'w', encoding='utf-8') as f:
    f.write(text)
