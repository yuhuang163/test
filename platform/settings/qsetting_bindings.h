#ifndef QSETTING_BINDINGS_H
#define QSETTING_BINDINGS_H

class QWidget;

/** 表驱动：从 ini 加载 / 保存 / 控件 tooltip（loadConfig、saveConfig、initSettingTooltips 共用） */
void loadQSettingTableBindings(QWidget* root);
void saveQSettingTableBindings(QWidget* root);
void applyQSettingTableBindingTooltips(QWidget* root);

#endif // QSETTING_BINDINGS_H
