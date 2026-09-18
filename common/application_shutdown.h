#ifndef APPLICATION_SHUTDOWN_H
#define APPLICATION_SHUTDOWN_H

/** 上位机关闭/OTA 替换前统一释放资源，避免 exe 或 COM 占用导致升级失败、二次打不开。 */
class ApplicationShutdown {
  public:
    /** 关窗/退出流程已开始（等待循环应立刻结束）。 */
    static bool isExiting();
    /** 新一轮测试/单步调试前清除退出标志（关窗未完成时避免误伤后续等待）。 */
    static void resetForNewSession();
    /** 停云端 Agent、刷日志（关窗路径不要等线程池，否则窗口会假死）。 */
    static void prepareForExit();
    /** 窗口已关后再短等 QtConcurrent HTTP，避免进程被 120s 超时拖住。 */
    static void waitBackgroundTasks(int timeoutMs = 800);
    /** OTA 替换前：停 Agent、刷日志，短等待后台任务（避免关窗 waitForDone 3s 拖慢升级）。 */
    static void prepareForOtaReplace();
    /** OTA：先 prepare，再延迟强杀进程树并 exit（box 工站 TotallyTask 也依赖 taskkill）。 */
    static void scheduleForceExitForOta(int delayMs = 800);
};

#endif // APPLICATION_SHUTDOWN_H
