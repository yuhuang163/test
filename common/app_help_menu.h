#ifndef APP_HELP_MENU_H
#define APP_HELP_MENU_H

#include <functional>

class QMenu;
class QMenuBar;
class QWidget;

/** 上位机统一「帮助」菜单：切换账号 / 更新·上传用例 / 上传日志 / 检查更新，并保证菜单在最右侧。 */
class AppHelpMenu {
  public:
    struct HostCallbacks {
        std::function<void()> onAccountSwitched; // 登录成功后刷新状态栏/设置可见性
        std::function<void()> onCheckUpdate;
    };

    /** 在 menuBar 末尾安装「帮助」；工站若随后还会 addAction，请再调 ensureRightmost（可用 singleShot(0)）。 */
    static QMenu* install(QMenuBar* menuBar, QWidget* dialogParent, const HostCallbacks& callbacks);

    /** 将「帮助」移到菜单栏最右侧（在其它菜单项添加之后调用）。 */
    static void ensureRightmost(QMenuBar* menuBar, QMenu* helpMenu);
};

#endif // APP_HELP_MENU_H
