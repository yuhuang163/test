#include "test_case_hook_registry.h"

#include <QHash>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

static QHash<QString, TestCaseHookFn>& hookTable() {
    static QHash<QString, TestCaseHookFn> table;
    return table;
}

void TestCaseHookRegistry::registerHook(const QString& hookId, TestCaseHookFn fn) {
    if (hookId.trimmed().isEmpty() || !fn)
        return;
    hookTable().insert(hookId.trimmed(), std::move(fn));
}

bool TestCaseHookRegistry::contains(const QString& hookId) {
    return hookTable().contains(hookId.trimmed());
}

QStringList TestCaseHookRegistry::hookIds() {
    return hookTable().keys();
}

bool TestCaseHookRegistry::invoke(const QString& hookId, QFreeWork* ctx) {
    const auto it = hookTable().constFind(hookId.trimmed());
    if (it == hookTable().cend() || !ctx)
        return false;
    it.value()(ctx);
    return true;
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
