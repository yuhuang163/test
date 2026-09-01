import re

with open('e:/C/test/mainwindow.ui', 'r', encoding='utf-8') as f:
    text = f.read()

# Replace size policies for group boxes
text = text.replace(
'''                         <property name="sizePolicy">
                          <sizepolicy hsizetype="Preferred" vsizetype="Fixed">
                           <horstretch>0</horstretch>
                           <verstretch>0</verstretch>
                          </sizepolicy>
                         </property>
                         <property name="title">
                          <string>参考图</string>''', 
'''                         <property name="sizePolicy">
                          <sizepolicy hsizetype="Preferred" vsizetype="Expanding">
                           <horstretch>0</horstretch>
                           <verstretch>0</verstretch>
                          </sizepolicy>
                         </property>
                         <property name="title">
                          <string>参考图</string>''')

text = text.replace(
'''                         <property name="sizePolicy">
                          <sizepolicy hsizetype="Preferred" vsizetype="Fixed">
                           <horstretch>0</horstretch>
                           <verstretch>0</verstretch>
                          </sizepolicy>
                         </property>
                         <property name="title">
                          <string>拍摄图（拖拽划定检测范围，红圈为疑似坏点）</string>''',
'''                         <property name="sizePolicy">
                          <sizepolicy hsizetype="Preferred" vsizetype="Expanding">
                           <horstretch>0</horstretch>
                           <verstretch>0</verstretch>
                          </sizepolicy>
                         </property>
                         <property name="title">
                          <string>拍摄图（拖拽划定检测范围，红圈为疑似坏点）</string>''')

text = text.replace(
'''                          <widget class="QLabel" name="label_refImage">
                           <property name="sizePolicy">
                            <sizepolicy hsizetype="Expanding" vsizetype="Fixed">
                             <horstretch>0</horstretch>
                             <verstretch>0</verstretch>
                            </sizepolicy>
                           </property>
                           <property name="minimumSize">
                            <size>
                             <width>160</width>
                             <height>120</height>
                            </size>
                           </property>
                           <property name="maximumSize">
                            <size>
                             <width>16777215</width>
                             <height>120</height>
                            </size>
                           </property>''',
'''                          <widget class="QLabel" name="label_refImage">
                           <property name="sizePolicy">
                            <sizepolicy hsizetype="Expanding" vsizetype="Expanding">
                             <horstretch>0</horstretch>
                             <verstretch>0</verstretch>
                            </sizepolicy>
                           </property>
                           <property name="minimumSize">
                            <size>
                             <width>320</width>
                             <height>240</height>
                            </size>
                           </property>
                           <property name="maximumSize">
                            <size>
                             <width>16777215</width>
                             <height>16777215</height>
                            </size>
                           </property>''')

text = text.replace(
'''                          <widget class="QLabel" name="label_currImage">
                           <property name="sizePolicy">
                            <sizepolicy hsizetype="Expanding" vsizetype="Fixed">
                             <horstretch>0</horstretch>
                             <verstretch>0</verstretch>
                            </sizepolicy>
                           </property>
                           <property name="minimumSize">
                            <size>
                             <width>160</width>
                             <height>120</height>
                            </size>
                           </property>
                           <property name="maximumSize">
                            <size>
                             <width>16777215</width>
                             <height>120</height>
                            </size>
                           </property>''',
'''                          <widget class="QLabel" name="label_currImage">
                           <property name="sizePolicy">
                            <sizepolicy hsizetype="Expanding" vsizetype="Expanding">
                             <horstretch>0</horstretch>
                             <verstretch>0</verstretch>
                            </sizepolicy>
                           </property>
                           <property name="minimumSize">
                            <size>
                             <width>320</width>
                             <height>240</height>
                            </size>
                           </property>
                           <property name="maximumSize">
                            <size>
                             <width>16777215</width>
                             <height>16777215</height>
                            </size>
                           </property>''')

with open('e:/C/test/mainwindow.ui', 'w', encoding='utf-8') as f:
    f.write(text)
