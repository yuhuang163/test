#include "factory_cloud_env.h"

#include "my_set/my_typedef.h"

#include <QComboBox>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QVector>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

struct FactoryCloudEnvPreset {
    QString key;
    QString baseUrl;
};

const QVector<FactoryCloudEnvPreset> kFactoryCloudEnvPresets = {
    {QStringLiteral("local"), QStringLiteral("http://127.0.0.1:8800")},
    {QStringLiteral("prod"), QStringLiteral("https://fctp.luteos.com")},
};

int envIndexForKey(QComboBox* combo, const QString& key) {
    if (!combo) {
        return -1;
    }
    for (int i = 0; i < combo->count(); ++i) {
        if (combo->itemData(i).toString() == key) {
            return i;
        }
    }
    return -1;
}

int envIndexForUrl(QComboBox* combo, const QString& url) {
    const QString n = FactoryCloudEnv::normalizeBaseUrl(url);
    for (const auto& p : kFactoryCloudEnvPresets) {
        if (FactoryCloudEnv::normalizeBaseUrl(p.baseUrl) == n) {
            return envIndexForKey(combo, p.key);
        }
    }
    return -1;
}

} // namespace

QString FactoryCloudEnv::customEnvKey() {
    return QStringLiteral("custom");
}

QString FactoryCloudEnv::normalizeBaseUrl(const QString& url) {
    QString s = url.trimmed();
    while (s.endsWith(QLatin1Char('/'))) {
        s.chop(1);
    }
    return s;
}

QString FactoryCloudEnv::baseUrlForKey(const QString& key) {
    for (const auto& p : kFactoryCloudEnvPresets) {
        if (p.key == key) {
            return p.baseUrl;
        }
    }
    return {};
}

void FactoryCloudEnv::populateEnvironmentCombo(QComboBox* combo) {
    if (!combo) {
        return;
    }
    combo->clear();
    combo->addItem(QStringLiteral("测试环境"), QStringLiteral("local"));
    combo->addItem(QStringLiteral("服务器"), QStringLiteral("prod"));
    combo->addItem(QStringLiteral("自定义"), customEnvKey());
}

void FactoryCloudEnv::updateBaseUrlEditEnabled(QComboBox* envCombo, QLineEdit* baseUrlEdit) {
    if (!envCombo || !baseUrlEdit) {
        return;
    }
    const QString key = envCombo->currentData().toString();
    baseUrlEdit->setReadOnly(key != customEnvKey());
}

void FactoryCloudEnv::applyEnvironmentSelection(QComboBox* envCombo, QLineEdit* baseUrlEdit) {
    if (!envCombo || !baseUrlEdit) {
        return;
    }
    const QString key = envCombo->currentData().toString();
    updateBaseUrlEditEnabled(envCombo, baseUrlEdit);
    if (key == customEnvKey() || key.isEmpty()) {
        return;
    }
    const QString url = baseUrlForKey(key);
    if (!url.isEmpty()) {
        baseUrlEdit->setText(url);
    }
}

void FactoryCloudEnv::loadToWidgets(QComboBox* envCombo, QLineEdit* baseUrlEdit) {
    if (!envCombo || !baseUrlEdit) {
        return;
    }
    const QString savedUrl =
        SETTINGS.value(QStringLiteral("FactoryCloud/BaseUrl"), baseUrlForKey(QStringLiteral("prod"))).toString();
    baseUrlEdit->setText(savedUrl);

    QSignalBlocker blocker(envCombo);
    QString envKey = SETTINGS.value(QStringLiteral("FactoryCloud/Environment")).toString().trimmed();
    int idx = envKey.isEmpty() ? -1 : envIndexForKey(envCombo, envKey);
    if (idx < 0) {
        idx = envIndexForUrl(envCombo, savedUrl);
    }
    if (idx < 0) {
        idx = envCombo->count() - 1;
    }
    envCombo->setCurrentIndex(idx);
    updateBaseUrlEditEnabled(envCombo, baseUrlEdit);
}

void FactoryCloudEnv::syncDerivedUrlsFromBaseUrl(const QString& baseUrl) {
    const QString trimmed = normalizeBaseUrl(baseUrl);
    if (trimmed.isEmpty()) {
        return;
    }
    SETTINGS.setValue(QStringLiteral("FactoryCloud/HostOtaCheckUrl"),
                      trimmed + QStringLiteral("/api/factory-tool/host-app/check"));
}

void FactoryCloudEnv::saveFromWidgets(QComboBox* envCombo, QLineEdit* baseUrlEdit) {
    if (!envCombo || !baseUrlEdit) {
        return;
    }
    const QString baseUrl = normalizeBaseUrl(baseUrlEdit->text());
    if (!baseUrl.isEmpty()) {
        SETTINGS.setValue(QStringLiteral("FactoryCloud/BaseUrl"), baseUrl);
    }
    SETTINGS.setValue(QStringLiteral("FactoryCloud/Environment"), envCombo->currentData().toString());
    syncDerivedUrlsFromBaseUrl(baseUrl);
    SETTINGS.sync();
}
