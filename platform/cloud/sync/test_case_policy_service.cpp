#include "test_case_policy_service.h"

#include "auth_service.h"
#include "factory_cloud_client.h"
#include "test_case_paths.h"
#include "test_case_store.h"

#include "my_set/my_typedef.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonObject>
#include <QSettings>
#include <QUrlQuery>
#include <QVariantMap>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

static QString stationScope(const QString& stationKey) {
    const QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    return key.isEmpty() ? QStringLiteral("_") : key;
}

/** 旧版：上位机设置.ini [TestCasePolicy/工站/键] */
static QString legacyPolicyKey(const QString& stationKey, const char* leaf) {
    return QStringLiteral("TestCasePolicy/%1/%2").arg(stationScope(stationKey), QString::fromLatin1(leaf));
}

static const char* const kPolicyLeaves[] = {
    "Phase",
    "BundleVersion",
    "CloudContentFingerprint",
    "SyncedContentFingerprint",
    "CloudProfileVersion",
    "CloudStationKey",
    "CloudDisplayName",
    "Dirty",
    "PendingReview",
    "FetchedAtMs",
    "ExemptionActive",
    "LastExemptionCode",
};

static QString metaPathFor(const QString& stationKey) {
    return TestCasePaths::profileMetaPath(stationKey);
}

static QString policyIniKey(const char* leaf) {
    return QStringLiteral("Policy/%1").arg(QString::fromLatin1(leaf));
}

/** 读策略：优先 profile.ini [Policy]，兼容迁移旧 TestCasePolicy */
static QVariant policyValue(const QString& stationKey, const char* leaf,
                            const QVariant& defaultValue = QVariant()) {
    const QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    if (key.isEmpty())
        return defaultValue;
    const QString path = metaPathFor(key);
    if (QFile::exists(path)) {
        QSettings meta(path, QSettings::IniFormat);
        meta.setIniCodec("UTF-8");
        const QString iniKey = policyIniKey(leaf);
        if (meta.contains(iniKey))
            return meta.value(iniKey, defaultValue);
        if (SETTINGS.contains(legacyPolicyKey(key, leaf))) {
            const QVariant legacy = SETTINGS.value(legacyPolicyKey(key, leaf));
            meta.setValue(iniKey, legacy);
            meta.sync();
            SETTINGS.remove(legacyPolicyKey(key, leaf));
            SETTINGS.sync();
            return legacy;
        }
    }
    return SETTINGS.value(legacyPolicyKey(key, leaf), defaultValue);
}

static void setPolicyValue(const QString& stationKey, const char* leaf, const QVariant& value) {
    const QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    if (key.isEmpty())
        return;
    const QString path = metaPathFor(key);
    if (path.isEmpty() || !QDir(QFileInfo(path).absolutePath()).exists()) {
        SETTINGS.setValue(legacyPolicyKey(key, leaf), value);
        SETTINGS.sync();
        return;
    }
    QSettings meta(path, QSettings::IniFormat);
    meta.setIniCodec("UTF-8");
    meta.setValue(policyIniKey(leaf), value);
    meta.sync();
    SETTINGS.remove(legacyPolicyKey(key, leaf));
}

static void removePolicyValue(const QString& stationKey, const char* leaf) {
    const QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    if (key.isEmpty())
        return;
    const QString path = metaPathFor(key);
    if (QFile::exists(path)) {
        QSettings meta(path, QSettings::IniFormat);
        meta.setIniCodec("UTF-8");
        meta.remove(policyIniKey(leaf));
        meta.sync();
    }
    SETTINGS.remove(legacyPolicyKey(key, leaf));
}

static void syncPolicySettings(const QString& stationKey) {
    const QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    if (key.isEmpty())
        return;
    const QString path = metaPathFor(key);
    if (QFile::exists(path)) {
        QSettings meta(path, QSettings::IniFormat);
        meta.setIniCodec("UTF-8");
        meta.sync();
    }
    SETTINGS.sync();
}

/** 指纹用：去掉 profile.ini 里本机 [Policy] 段，避免策略缓存改动影响内容指纹 */
static QByteArray stripPolicySectionForFingerprint(QByteArray data) {
    data.replace("\r\n", "\n");
    data.replace('\r', '\n');
    const QByteArray marker = QByteArrayLiteral("[Policy]");
    int start = data.indexOf(marker);
    if (start < 0)
        return data;
    while (start > 0 && (data.at(start - 1) == '\n' || data.at(start - 1) == '\r'))
        --start;
    int scan = start + marker.size();
    int end = data.size();
    while (scan < data.size()) {
        if (data.at(scan) == '\n' && scan + 1 < data.size() && data.at(scan + 1) == '[') {
            end = scan + 1;
            break;
        }
        ++scan;
    }
    data.remove(start, end - start);
    while (data.endsWith('\n') && data.size() >= 2 && data.endsWith("\n\n"))
        data.chop(1);
    return data;
}

static QString fileSha256Hex(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QByteArray data = file.readAll();
    if (QFileInfo(path).fileName().compare(QStringLiteral("profile.ini"), Qt::CaseInsensitive) == 0)
        data = stripPolicySectionForFingerprint(data);
    else {
        data.replace("\r\n", "\n");
        data.replace('\r', '\n');
    }
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QVariantMap TestCasePolicyService::detachPolicyForUpload(const QString& stationKey) {
    QVariantMap saved;
    const QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    if (key.isEmpty() || !QFile::exists(metaPathFor(key)))
        return saved;
    QSettings meta(metaPathFor(key), QSettings::IniFormat);
    meta.setIniCodec("UTF-8");
    meta.beginGroup(QStringLiteral("Policy"));
    const QStringList keys = meta.childKeys();
    for (const QString& k : keys)
        saved.insert(k, meta.value(k));
    meta.endGroup();
    meta.remove(QStringLiteral("Policy"));
    meta.sync();
    return saved;
}

void TestCasePolicyService::reattachPolicyAfterUpload(const QString& stationKey, const QVariantMap& saved) {
    if (saved.isEmpty())
        return;
    const QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    if (key.isEmpty() || !QFile::exists(metaPathFor(key)))
        return;
    QSettings meta(metaPathFor(key), QSettings::IniFormat);
    meta.setIniCodec("UTF-8");
    for (auto it = saved.constBegin(); it != saved.constEnd(); ++it)
        meta.setValue(QStringLiteral("Policy/%1").arg(it.key()), it.value());
    meta.sync();
}

QString TestCasePolicyService::localProfileFingerprint(const QString& stationKey) {
    const QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    if (key.isEmpty()) {
        return {};
    }
    const QString profileDir = TestCasePaths::profileDir(key);
    if (!QDir(profileDir).exists()) {
        return {};
    }
    QCryptographicHash digest(QCryptographicHash::Sha256);
    QStringList rels;
    QDirIterator it(profileDir, QStringList{QStringLiteral("*.ini")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString rel =
            QDir(profileDir).relativeFilePath(it.filePath()).replace(QLatin1Char('\\'), QLatin1Char('/'));
        rels.append(rel);
    }
    rels.sort();
    for (const QString& rel : rels) {
        const QString abs = QDir(profileDir).filePath(rel);
        const QString sha = fileSha256Hex(abs);
        digest.addData(rel.toUtf8());
        digest.addData("\0", 1);
        digest.addData(sha.toUtf8());
        digest.addData("\0", 1);
    }
    return QString::fromLatin1(digest.result().toHex());
}

void TestCasePolicyService::clearCache(const QString& stationKey) {
    if (stationKey.trimmed().isEmpty()) {
        SETTINGS.beginGroup(QStringLiteral("TestCasePolicy"));
        SETTINGS.remove(QString());
        SETTINGS.endGroup();
        for (const char* leaf : kPolicyLeaves) {
            SETTINGS.remove(QStringLiteral("TestCasePolicy/%1").arg(QString::fromLatin1(leaf)));
        }
        SETTINGS.sync();
        for (const TestFlowStationEntry& entry : TestCaseStore::loadFlowStationCatalog()) {
            if (!QFile::exists(metaPathFor(entry.key)))
                continue;
            QSettings meta(metaPathFor(entry.key), QSettings::IniFormat);
            meta.setIniCodec("UTF-8");
            meta.remove(QStringLiteral("Policy"));
            meta.sync();
        }
        return;
    }
    const QString key = stationScope(stationKey);
    if (QFile::exists(metaPathFor(key))) {
        QSettings meta(metaPathFor(key), QSettings::IniFormat);
        meta.setIniCodec("UTF-8");
        meta.remove(QStringLiteral("Policy"));
        meta.sync();
    }
    SETTINGS.beginGroup(QStringLiteral("TestCasePolicy/%1").arg(key));
    SETTINGS.remove(QString());
    SETTINGS.endGroup();
    SETTINGS.sync();
}

void TestCasePolicyService::markDirtyAfterUiEdit(const QString& stationKey) {
    QString key = stationKey.trimmed();
    if (key.isEmpty()) {
        key = TestCaseStore::loadSelectedFlowStationKey();
    }
    setPolicyValue(key, "Dirty", true);
    setPolicyValue(key, "SyncedContentFingerprint", QString());
    setPolicyValue(key, "ExemptionActive", false);
    syncPolicySettings(key);
}

void TestCasePolicyService::markPendingAfterUpload(const QString& stationKey) {
    const QString key = stationKey.trimmed();
    setPolicyValue(key, "PendingReview", true);
    setPolicyValue(key, "Dirty", true);
    setPolicyValue(key, "SyncedContentFingerprint", QString());
    syncPolicySettings(key);
}

void TestCasePolicyService::markSyncedClean(const QString& stationKey, const QString& contentFingerprint,
                                            const QString& bundleVersion) {
    const QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    QString fp = contentFingerprint.trimmed().toLower();
    if (fp.isEmpty() && !key.isEmpty()) {
        fp = localProfileFingerprint(key).toLower();
    }
    setPolicyValue(key, "SyncedContentFingerprint", fp);
    setPolicyValue(key, "CloudContentFingerprint", fp);
    if (!bundleVersion.trimmed().isEmpty()) {
        setPolicyValue(key, "BundleVersion", bundleVersion.trimmed());
    }
    setPolicyValue(key, "Dirty", false);
    setPolicyValue(key, "PendingReview", false);
    setPolicyValue(key, "ExemptionActive", false);
    setPolicyValue(key, "FetchedAtMs", QDateTime::currentMSecsSinceEpoch());
    syncPolicySettings(key);
}

static TestCasePolicyService::Phase phaseFromString(const QString& text) {
    return text.trimmed().compare(QStringLiteral("mass"), Qt::CaseInsensitive) == 0
               ? TestCasePolicyService::Phase::Mass
               : TestCasePolicyService::Phase::Trial;
}

static bool refreshPolicyFromCloud(const QString& stationKey, const QString& displayName,
                                   QString* exemptionCode, QString* errorOut) {
    QUrlQuery query;
    if (!stationKey.trimmed().isEmpty()) {
        query.addQueryItem(QStringLiteral("stationKey"), stationKey.trimmed());
    }
    if (!displayName.trimmed().isEmpty()) {
        query.addQueryItem(QStringLiteral("displayName"), displayName.trimmed());
    }
    if (exemptionCode && !exemptionCode->trimmed().isEmpty()) {
        query.addQueryItem(QStringLiteral("exemptionCode"), exemptionCode->trimmed());
    }
    // 开测门禁走 UI 线程嵌套事件循环：短超时，避免 API 挂死时回车像“没反应”
    constexpr int kGatePolicyTimeoutMs = 5000;
    const FactoryCloudClient::ApiResult api =
        FactoryCloudClient::get(QStringLiteral("/test-cases/policy"), query, kGatePolicyTimeoutMs);
    if (!api.ok) {
        if (errorOut) {
            *errorOut = api.message.isEmpty() ? QStringLiteral("拉取用例策略失败") : api.message;
        }
        return false;
    }
    const QJsonObject data = api.data;
    const QString key = stationKey.trimmed();
    setPolicyValue(key, "Phase", data.value(QStringLiteral("phase")).toString(QStringLiteral("trial")));
    setPolicyValue(key, "BundleVersion", data.value(QStringLiteral("bundleVersion")).toString());
    setPolicyValue(key, "CloudContentFingerprint",
                   data.value(QStringLiteral("contentFingerprint")).toString().trimmed().toLower());
    setPolicyValue(key, "CloudProfileVersion",
                   data.value(QStringLiteral("profileVersion")).toString().trimmed());
    setPolicyValue(key, "CloudStationKey", data.value(QStringLiteral("stationKey")).toString().trimmed());
    setPolicyValue(key, "CloudDisplayName",
                   data.value(QStringLiteral("displayName")).toString().trimmed());
    setPolicyValue(key, "ExemptionActive", data.value(QStringLiteral("exemptionActive")).toBool());
    setPolicyValue(key, "FetchedAtMs", QDateTime::currentMSecsSinceEpoch());
    syncPolicySettings(key);
    return true;
}

/** 合入后云端常只改 profile.ini 元数据：对齐后指纹一致则清 dirty/pending，免整包下载 */
static bool adoptCloudContentIfLocalMatches(const QString& stationKey) {
    const QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    if (key.isEmpty())
        return false;
    const QString cloudFp =
        policyValue(key, "CloudContentFingerprint").toString().trimmed().toLower();
    if (cloudFp.isEmpty())
        return false;
    const QString bundleVersion = policyValue(key, "BundleVersion").toString();

    QString localFp = TestCasePolicyService::localProfileFingerprint(key).toLower();
    if (!localFp.isEmpty() && localFp == cloudFp) {
        TestCasePolicyService::markSyncedClean(key, cloudFp, bundleVersion);
        return true;
    }

    const QString cloudVer = policyValue(key, "CloudProfileVersion").toString().trimmed();
    const QString cloudKey = policyValue(key, "CloudStationKey").toString().trimmed();
    const QString cloudName = policyValue(key, "CloudDisplayName").toString().trimmed();
    if (cloudVer.isEmpty() && cloudKey.isEmpty() && cloudName.isEmpty())
        return false;

    const QString metaPath = TestCasePaths::profileMetaPath(key);
    if (!QFile::exists(metaPath))
        return false;

    QSettings meta(metaPath, QSettings::IniFormat);
    meta.setIniCodec("UTF-8");
    const QString oldVer = meta.value(QStringLiteral("Profile/ProfileVersion")).toString();
    const QString oldKey = meta.value(QStringLiteral("Profile/StationKey")).toString();
    const QString oldName = meta.value(QStringLiteral("Profile/DisplayName")).toString();
    if (!cloudVer.isEmpty())
        meta.setValue(QStringLiteral("Profile/ProfileVersion"), cloudVer);
    if (!cloudKey.isEmpty())
        meta.setValue(QStringLiteral("Profile/StationKey"), cloudKey);
    if (!cloudName.isEmpty())
        meta.setValue(QStringLiteral("Profile/DisplayName"), cloudName);
    meta.sync();

    localFp = TestCasePolicyService::localProfileFingerprint(key).toLower();
    if (!localFp.isEmpty() && localFp == cloudFp) {
        TestCasePolicyService::markSyncedClean(key, cloudFp, bundleVersion);
        return true;
    }

    // 内容仍不一致：回滚元数据，避免仅因探测改坏本地版本号
    meta.setValue(QStringLiteral("Profile/ProfileVersion"), oldVer);
    meta.setValue(QStringLiteral("Profile/StationKey"), oldKey);
    meta.setValue(QStringLiteral("Profile/DisplayName"), oldName);
    meta.sync();
    return false;
}

TestCasePolicyService::GateResult TestCasePolicyService::evaluateStartGate(const QString& stationKey,
                                                                           const QString& displayName,
                                                                           QString* exemptionCode) {
    GateResult result;
    // 离线调试无法拉策略/验豁免码，与其它云端功能一致直接放行
    if (AuthService::isOfflineSession()) {
        result.message = QStringLiteral("离线测试模式，跳过量产用例卡控");
        return result;
    }
    const QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    const QString name =
        displayName.trimmed().isEmpty() ? TestCaseStore::flowStationDisplayName(key) : displayName.trimmed();

    bool dirty = policyValue(key, "Dirty", false).toBool();
    bool pending = policyValue(key, "PendingReview", false).toBool();
    // 先用缓存指纹本地对齐，避免 API 慢/挂时回车卡在刷新里、界面却仍能动
    if (dirty || pending)
        adoptCloudContentIfLocalMatches(key);
    dirty = policyValue(key, "Dirty", false).toBool();
    pending = policyValue(key, "PendingReview", false).toBool();

    // 开测传入码优先；否则复用本工站上次校验通过的豁免码
    QString codeStorage;
    QString* codeToUse = exemptionCode;
    if (!codeToUse || codeToUse->trimmed().isEmpty()) {
        const QString last = policyValue(key, "LastExemptionCode").toString().trimmed();
        if (!last.isEmpty()) {
            codeStorage = last;
            codeToUse = &codeStorage;
        }
    }

    // 网页可随时改试产/量产：开测须拉策略；仅 10s 内连点开测复用缓存
    const qint64 fetchedAt = policyValue(key, "FetchedAtMs", 0).toLongLong();
    const bool cacheVeryFresh =
        fetchedAt > 0 && (QDateTime::currentMSecsSinceEpoch() - fetchedAt) < 10000LL;
    const bool hasExemptionInput = codeToUse && !codeToUse->trimmed().isEmpty();

    QString refreshError;
    bool refreshed = false;
    if (!cacheVeryFresh || dirty || pending || hasExemptionInput) {
        refreshed = refreshPolicyFromCloud(key, name, codeToUse, &refreshError);
    }

    // 上传并合入后本地内容已等于云端：自动视为已同步（含仅差合入写入的 ProfileVersion）
    if (refreshed || dirty || pending)
        adoptCloudContentIfLocalMatches(key);

    dirty = policyValue(key, "Dirty", false).toBool();
    pending = policyValue(key, "PendingReview", false).toBool();
    const QString localFp = localProfileFingerprint(key).toLower();

    const Phase phase =
        phaseFromString(policyValue(key, "Phase", QStringLiteral("trial")).toString());
    result.phase = phase;
    const bool exemptionActive = policyValue(key, "ExemptionActive", false).toBool();
    const QString cloudFp =
        policyValue(key, "CloudContentFingerprint").toString().trimmed().toLower();
    const QString syncedFpNow =
        policyValue(key, "SyncedContentFingerprint").toString().trimmed().toLower();

    const bool fingerprintMismatch =
        !cloudFp.isEmpty() && !localFp.isEmpty() && cloudFp != localFp;
    const bool notSyncedClean =
        dirty || pending || syncedFpNow.isEmpty() || (!localFp.isEmpty() && syncedFpNow != localFp) ||
        fingerprintMismatch;

    if (hasExemptionInput && refreshed && !exemptionActive) {
        removePolicyValue(key, "LastExemptionCode");
        syncPolicySettings(key);
    }

    if (exemptionActive) {
        if (hasExemptionInput) {
            setPolicyValue(key, "LastExemptionCode", codeToUse->trimmed());
            syncPolicySettings(key);
        }
        result.allowed = true;
        // 豁免仅记日志，避免每台都点「试产用例提示」打断节拍
        result.warningOnly = false;
        result.message = QStringLiteral("工站「%1」已使用临时豁免，允许开测（量产请尽快对齐正式用例）")
                             .arg(name.isEmpty() ? key : name);
        return result;
    }

    if (phase == Phase::Trial) {
        result.allowed = true;
        if (notSyncedClean) {
            result.warningOnly = true;
            QStringList reasons;
            if (dirty)
                reasons << QStringLiteral("本地用例已修改");
            if (pending)
                reasons << QStringLiteral("草稿待网页审核合入");
            if (fingerprintMismatch)
                reasons << QStringLiteral("与云端正式版不一致");
            if (reasons.isEmpty())
                reasons << QStringLiteral("尚未对齐正式版");
            result.message =
                QStringLiteral("工站「%1」为试产：%2，仍可开测。量产前请上传并在网页合入；内容一致后无需再下载。")
                    .arg(name.isEmpty() ? key : name, reasons.join(QStringLiteral("；")));
        } else if (!refreshed && !refreshError.isEmpty()) {
            result.warningOnly = true;
            result.message = QStringLiteral("工站「%1」试产：策略云端刷新失败（%2），按缓存继续开测")
                                 .arg(name.isEmpty() ? key : name, refreshError);
        }
        return result;
    }

    if (notSyncedClean) {
        result.allowed = false;
        QStringList reasons;
        if (dirty)
            reasons << QStringLiteral("本地用例已修改，须重新上传并等待网页合入");
        if (pending)
            reasons << QStringLiteral("草稿待网页合入");
        if (fingerprintMismatch || (!cloudFp.isEmpty() && syncedFpNow != cloudFp))
            reasons << QStringLiteral("本地与云端正式用例不一致（若网页另有修改请下载，或确认已合入你上传的版本）");
        if (syncedFpNow.isEmpty() && !dirty && !pending)
            reasons << QStringLiteral("尚未与云端正式用例对齐");
        if (reasons.isEmpty())
            reasons << QStringLiteral("用例未与正式版对齐");
        result.message = QStringLiteral("工站「%1」为量产，禁止开测：%2")
                             .arg(name.isEmpty() ? key : name, reasons.join(QStringLiteral("；")));
        if (hasExemptionInput && refreshed)
            result.message += QStringLiteral("（豁免码无效、已过期或已撤销）");
        if (!refreshed && !refreshError.isEmpty()) {
            result.message += QStringLiteral("（策略刷新失败：%1）").arg(refreshError);
        }
        return result;
    }

    // 量产且已对齐正式版：允许开测（这是预期，不是漏卡）
    result.allowed = true;
    result.message = QStringLiteral("工站「%1」为量产且已对齐云端正式版，允许开测")
                         .arg(name.isEmpty() ? key : name);
    return result;
}
