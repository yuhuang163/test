#ifndef APPLICATION_SHUTDOWN_H
#define APPLICATION_SHUTDOWN_H

/** 上位机关闭/OTA 替换前统一释放资源，避免 exe 或 COM 占用导致升级失败、二次打不开。 */
class ApplicationShutdown {
  public:
    /** 停云端 Agent、结束主循环、关串口/仪器等（可重复调用）。 */
    static void prepareForExit();
    /** OTA：先 prepare，再延迟强杀进程树并 exit（box 工站 TotallyTask 也依赖 taskkill）。 */
    static void scheduleForceExitForOta(int delayMs = 800);
};

#endif // APPLICATION_SHUTDOWN_H
