#ifndef FACTORY_CLOUD_ENV_H
#define FACTORY_CLOUD_ENV_H

#include <QString>

class QComboBox;
class QLineEdit;

/** 路特产线云平台环境（FactoryCloud/Environment、BaseUrl） */
class FactoryCloudEnv {
  public:
    static QString customEnvKey();

    static void populateEnvironmentCombo(QComboBox* combo);
    static QString baseUrlForKey(const QString& key);
    static QString normalizeBaseUrl(const QString& url);

    /** 从 SETTINGS 回填到下拉与 BaseUrl 输入框 */
    static void loadToWidgets(QComboBox* envCombo, QLineEdit* baseUrlEdit);
    /** 将当前选择写入 SETTINGS，并同步 OTA 检查地址等派生项 */
    static void saveFromWidgets(QComboBox* envCombo, QLineEdit* baseUrlEdit);

    /** 非「自定义」时按预设刷新 BaseUrl；自定义时允许手改 */
    static void applyEnvironmentSelection(QComboBox* envCombo, QLineEdit* baseUrlEdit);
    static void updateBaseUrlEditEnabled(QComboBox* envCombo, QLineEdit* baseUrlEdit);
    static void syncDerivedUrlsFromBaseUrl(const QString& baseUrl);
};

#endif // FACTORY_CLOUD_ENV_H
