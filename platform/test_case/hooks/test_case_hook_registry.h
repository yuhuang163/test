#ifndef PLATFORM_TEST_CASE_HOOK_REGISTRY_H
#define PLATFORM_TEST_CASE_HOOK_REGISTRY_H

#include <functional>
#include <QString>
#include <QStringList>

class QFreeWork;

using TestCaseHookFn = std::function<void(QFreeWork*)>;

class TestCaseHookRegistry {
  public:
    static void registerHook(const QString& hookId, TestCaseHookFn fn);
    static bool contains(const QString& hookId);
    static QStringList hookIds();
    static bool invoke(const QString& hookId, QFreeWork* ctx);
};

/** 自由工站目录钩子（幂等，实现于 qfreework_case_hooks.cpp）。 */
void registerQFreeWorkCatalogTestCaseHooks();

#endif // PLATFORM_TEST_CASE_HOOK_REGISTRY_H
