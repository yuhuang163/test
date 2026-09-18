#include "host_ota_vbs.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStringList>
#include <QTextStream>
#include <QTimer>

#include <string>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#endif

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

static QString vbsQuote(const QString& path) {
    return QDir::toNativeSeparators(path).replace(QLatin1Char('"'), QStringLiteral("\"\""));
}

static QString collectMatchingProcessPidsCsv(const QString& imageName) {
#ifdef Q_OS_WIN
    QStringList pids;
    const std::wstring nameW = imageName.toStdWString();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return {};
    }
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, nameW.c_str()) == 0) {
                pids << QString::number(static_cast<qint64>(pe.th32ProcessID));
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pids.join(QLatin1Char(','));
#else
    Q_UNUSED(imageName);
    return {};
#endif
}

static void appendTryFileOps(QTextStream& vbs) {
    vbs << "Dim lastErr\r\n";
    vbs << "lastErr = 0\r\n";
    vbs << "Function TryEnsureDir(p)\r\n";
    vbs << "  Dim parts, cur, j\r\n";
    vbs << "  TryEnsureDir = True\r\n";
    vbs << "  If Len(p) = 0 Then Exit Function\r\n";
    vbs << "  parts = Split(p, \"\\\")\r\n";
    vbs << "  cur = parts(0)\r\n";
    vbs << "  For j = 1 To UBound(parts)\r\n";
    vbs << "    cur = cur & \"\\\" & parts(j)\r\n";
    vbs << "    If Not fso.FolderExists(cur) Then\r\n";
    vbs << "      On Error Resume Next\r\n";
    vbs << "      Err.Clear\r\n";
    vbs << "      fso.CreateFolder(cur)\r\n";
    vbs << "      lastErr = Err.Number\r\n";
    vbs << "      On Error GoTo 0\r\n";
    vbs << "      If lastErr <> 0 Then\r\n";
    vbs << "        TryEnsureDir = False\r\n";
    vbs << "        Exit Function\r\n";
    vbs << "      End If\r\n";
    vbs << "    End If\r\n";
    vbs << "  Next\r\n";
    vbs << "End Function\r\n";
    vbs << "Function TryDeleteFile(p)\r\n";
    vbs << "  TryDeleteFile = True\r\n";
    vbs << "  If Not fso.FileExists(p) Then Exit Function\r\n";
    vbs << "  On Error Resume Next\r\n";
    vbs << "  Err.Clear\r\n";
    vbs << "  fso.DeleteFile p, True\r\n";
    vbs << "  lastErr = Err.Number\r\n";
    vbs << "  On Error GoTo 0\r\n";
    vbs << "  If lastErr <> 0 Then TryDeleteFile = False\r\n";
    vbs << "End Function\r\n";
    vbs << "Function TryMoveFile(src, dst)\r\n";
    vbs << "  On Error Resume Next\r\n";
    vbs << "  Err.Clear\r\n";
    vbs << "  fso.MoveFile src, dst\r\n";
    vbs << "  lastErr = Err.Number\r\n";
    vbs << "  On Error GoTo 0\r\n";
    vbs << "  TryMoveFile = (lastErr = 0)\r\n";
    vbs << "End Function\r\n";
}

static void appendKillHostProcesses(QTextStream& vbs) {
    vbs << "Sub KillHostProcesses\r\n";
    vbs << "  Dim pidArr, p, nKilled\r\n";
    vbs << "  nKilled = 0\r\n";
    vbs << "  pidArr = Split(killPids, \",\")\r\n";
    vbs << "  On Error Resume Next\r\n";
    vbs << "  For Each p In pidArr\r\n";
    vbs << "    p = Trim(p)\r\n";
    vbs << "    If Len(p) > 0 Then\r\n";
    vbs << "      sh.Run \"cmd /c taskkill /F /PID \" & p & \" /T >nul 2>&1\", 0, False\r\n";
    vbs << "      nKilled = nKilled + 1\r\n";
    vbs << "    End If\r\n";
    vbs << "  Next\r\n";
    vbs << "  If nKilled = 0 Then\r\n";
    vbs << "    sh.Run \"cmd /c taskkill /F /IM \"\"\" & exeName & \"\"\" /T >nul 2>&1\", 0, False\r\n";
    vbs << "  End If\r\n";
    vbs << "  On Error GoTo 0\r\n";
    vbs << "End Sub\r\n";
}

static void appendGetHostPidCsv(QTextStream& vbs) {
    vbs << "Function GetHostPidCsv\r\n";
    vbs << "  Dim f, tmp, line, parts, pids\r\n";
    vbs << "  GetHostPidCsv = \"\"\r\n";
    vbs << "  pids = \"\"\r\n";
    vbs << "  On Error Resume Next\r\n";
    vbs << "  tmp = fso.BuildPath(fso.GetSpecialFolder(2), \"ota_pids_\" & pid & \"_\" & Replace(CStr(Timer), \".\", \"\") & \".txt\")\r\n";
    vbs << "  sh.Run \"cmd /c tasklist /NH /FI \"\"IMAGENAME eq \" & exeName & \"\"\" > \"\"\" & tmp & \"\"\" 2>nul\", 0, True\r\n";
    vbs << "  If fso.FileExists(tmp) Then\r\n";
    vbs << "    Set f = fso.OpenTextFile(tmp, 1, False)\r\n";
    vbs << "    Do While Not f.AtEndOfStream\r\n";
    vbs << "      line = Trim(f.ReadLine)\r\n";
    vbs << "      If Len(line) > 0 Then\r\n";
    vbs << "        If InStr(1, line, exeName, vbTextCompare) = 1 Then\r\n";
    vbs << "          Do While InStr(line, \"  \") > 0\r\n";
    vbs << "            line = Replace(line, \"  \", \" \")\r\n";
    vbs << "          Loop\r\n";
    vbs << "          parts = Split(line, \" \")\r\n";
    vbs << "          If UBound(parts) >= 1 Then\r\n";
    vbs << "            If IsNumeric(parts(1)) Then\r\n";
    vbs << "              If Len(pids) > 0 Then pids = pids & \",\"\r\n";
    vbs << "              pids = pids & parts(1)\r\n";
    vbs << "            End If\r\n";
    vbs << "          End If\r\n";
    vbs << "        End If\r\n";
    vbs << "      End If\r\n";
    vbs << "    Loop\r\n";
    vbs << "    f.Close\r\n";
    vbs << "    fso.DeleteFile tmp, True\r\n";
    vbs << "  End If\r\n";
    vbs << "  On Error GoTo 0\r\n";
    vbs << "  GetHostPidCsv = pids\r\n";
    vbs << "End Function\r\n";
    vbs << "Function PidListed(pidVal, csv)\r\n";
    vbs << "  Dim arr, x\r\n";
    vbs << "  PidListed = False\r\n";
    vbs << "  arr = Split(csv, \",\")\r\n";
    vbs << "  For Each x In arr\r\n";
    vbs << "    If Trim(x) = pidVal Then\r\n";
    vbs << "      PidListed = True\r\n";
    vbs << "      Exit Function\r\n";
    vbs << "    End If\r\n";
    vbs << "  Next\r\n";
    vbs << "End Function\r\n";
    vbs << "Function IsProcessAlive\r\n";
    vbs << "  IsProcessAlive = (Len(GetHostPidCsv()) > 0)\r\n";
    vbs << "End Function\r\n";
    // 只认杀掉旧进程之后新出现的 PID，避免同名残留进程被当成升级成功
    vbs << "Function HasLaunchedHostProcess\r\n";
    vbs << "  Dim cur, arr, p\r\n";
    vbs << "  HasLaunchedHostProcess = False\r\n";
    vbs << "  cur = GetHostPidCsv()\r\n";
    vbs << "  arr = Split(cur, \",\")\r\n";
    vbs << "  For Each p In arr\r\n";
    vbs << "    p = Trim(p)\r\n";
    vbs << "    If Len(p) > 0 Then\r\n";
    vbs << "      If Not PidListed(p, excludePids) Then\r\n";
    vbs << "        HasLaunchedHostProcess = True\r\n";
    vbs << "        Exit Function\r\n";
    vbs << "      End If\r\n";
    vbs << "    End If\r\n";
    vbs << "  Next\r\n";
    vbs << "End Function\r\n";
}

static void appendLaunchHostExe(QTextStream& vbs) {
    vbs << "Function LaunchHostExe\r\n";
    vbs << "  On Error Resume Next\r\n";
    vbs << "  Err.Clear\r\n";
    vbs << "  sh.Run \"\"\"\" & sh.CurrentDirectory & \"\\\" & exeName & \"\"\"\", 1, False\r\n";
    vbs << "  lastErr = Err.Number\r\n";
    vbs << "  On Error GoTo 0\r\n";
    vbs << "  LaunchHostExe = (lastErr = 0)\r\n";
    vbs << "End Function\r\n";
}

static void appendWaitProcessAlive(QTextStream& vbs) {
    vbs << "Function WaitProcessAlive\r\n";
    vbs << "  Dim hc\r\n";
    vbs << "  WaitProcessAlive = False\r\n";
    vbs << "  For hc = 1 To 12\r\n";
    vbs << "    WScript.Sleep 300\r\n";
    vbs << "    If HasLaunchedHostProcess Then\r\n";
    vbs << "      WaitProcessAlive = True\r\n";
    vbs << "      Exit Function\r\n";
    vbs << "    End If\r\n";
    vbs << "  Next\r\n";
    vbs << "End Function\r\n";
}

static void appendShowOtaFail(QTextStream& vbs) {
    vbs << "Sub ShowOtaFail(msg)\r\n";
    vbs << "  LogStep \"fail popup: \" & msg\r\n";
    vbs << "  If Not IsProcessAlive Then\r\n";
    vbs << "    LaunchHostExe\r\n";
    vbs << "    WScript.Sleep 800\r\n";
    vbs << "  End If\r\n";
    vbs << QStringLiteral("  MsgBox msg & vbCrLf & vbCrLf & \"日志：\" & logPath, 16, \"上位机升级失败\"\r\n");
    vbs << "End Sub\r\n";
}

static void appendLogStep(QTextStream& vbs) {
    vbs << "Dim tStart, logf\r\n";
    vbs << "tStart = Timer\r\n";
    vbs << "Sub LogStep(msg)\r\n";
    vbs << "  Dim dt\r\n";
    vbs << "  dt = Timer - tStart\r\n";
    vbs << "  If dt < 0 Then dt = dt + 86400\r\n";
    vbs << "  logf.WriteLine Now & \" [vbs +\" & Round(dt, 3) & \"s] \" & msg\r\n";
    vbs << "End Sub\r\n";
}

static void appendCommonHelpers(QTextStream& vbs) {
    appendLaunchHostExe(vbs);
    appendKillHostProcesses(vbs);
    appendGetHostPidCsv(vbs);
    appendWaitProcessAlive(vbs);
    appendShowOtaFail(vbs);
}

static void appendReplaceBody(QTextStream& vbs) {
    vbs << "Sub RollbackAll\r\n";
    vbs << "  Dim k\r\n";
    vbs << "  rollbackFail = False\r\n";
    vbs << "  For k = 0 To nFiles - 1\r\n";
    vbs << "    If existed(k) Then\r\n";
    vbs << "      If fso.FileExists(backup(k)) Then\r\n";
    vbs << "        If Not TryEnsureDir(fso.GetParentFolderName(target(k))) Then\r\n";
    vbs << "          rollbackFail = True\r\n";
    vbs << "          LogStep \"rollback ensureDir fail \" & target(k) & \" err=\" & lastErr\r\n";
    vbs << "        ElseIf fso.FileExists(target(k)) And Not TryDeleteFile(target(k)) Then\r\n";
    vbs << "          rollbackFail = True\r\n";
    vbs << "          LogStep \"rollback delete target fail \" & target(k) & \" err=\" & lastErr\r\n";
    vbs << "        ElseIf Not TryMoveFile(backup(k), target(k)) Then\r\n";
    vbs << "          rollbackFail = True\r\n";
    vbs << "          LogStep \"rollback move fail \" & backup(k) & \" err=\" & lastErr\r\n";
    vbs << "        End If\r\n";
    vbs << "      Else\r\n";
    vbs << "        rollbackFail = True\r\n";
    vbs << "        LogStep \"rollback missing backup \" & backup(k)\r\n";
    vbs << "      End If\r\n";
    vbs << "    Else\r\n";
    vbs << "      If fso.FileExists(target(k)) Then\r\n";
    vbs << "        If Not TryDeleteFile(target(k)) Then\r\n";
    vbs << "          rollbackFail = True\r\n";
    vbs << "          LogStep \"rollback delete new file fail \" & target(k) & \" err=\" & lastErr\r\n";
    vbs << "        Else\r\n";
    vbs << "          LogStep \"rollback removed new file \" & target(k)\r\n";
    vbs << "        End If\r\n";
    vbs << "      End If\r\n";
    vbs << "    End If\r\n";
    vbs << "  Next\r\n";
    vbs << "  LogStep \"rollback done fail=\" & rollbackFail\r\n";
    vbs << "End Sub\r\n";
    appendCommonHelpers(vbs);
    vbs << "LogStep \"taskkill begin pids=\" & killPids\r\n";
    vbs << "KillHostProcesses\r\n";
    vbs << "LogStep \"taskkill async sent, sleep 400ms\"\r\n";
    vbs << "WScript.Sleep 400\r\n";
    vbs << "excludePids = GetHostPidCsv()\r\n";
    vbs << "LogStep \"exclude leftover pids=\" & excludePids\r\n";
    vbs << "Dim i, okMove, retry, alive, rollbackFail, quitCode\r\n";
    vbs << "rollbackFail = False\r\n";
    vbs << "quitCode = 0\r\n";
    vbs << "For i = 0 To nFiles - 1\r\n";
    vbs << "  LogStep \"file \" & (i + 1) & \"/\" & nFiles & \" begin \" & relPath(i)\r\n";
    vbs << "  If Not TryEnsureDir(fso.GetParentFolderName(backup(i))) Then\r\n";
    vbs << "    LogStep \"ensure backup dir FAILED \" & backup(i) & \" err=\" & lastErr\r\n";
    vbs << QStringLiteral("    ShowOtaFail \"无法创建备份目录：\" & relPath(i) & \"，请查看日志后手动打开上位机。\"\r\n");
    vbs << "    quitCode = 2\r\n";
    vbs << "    Exit For\r\n";
    vbs << "  End If\r\n";
    vbs << "  If fso.FileExists(backup(i)) Then\r\n";
    vbs << "    If Not TryDeleteFile(backup(i)) Then\r\n";
    vbs << "      LogStep \"delete old backup FAILED \" & backup(i) & \" err=\" & lastErr\r\n";
    vbs << QStringLiteral("      ShowOtaFail \"无法清理旧备份：\" & relPath(i) & \"，请查看日志后手动打开上位机。\"\r\n");
    vbs << "      quitCode = 2\r\n";
    vbs << "      Exit For\r\n";
    vbs << "    End If\r\n";
    vbs << "  End If\r\n";
    vbs << "  If existed(i) Then\r\n";
    vbs << "    If Not fso.FileExists(target(i)) Then\r\n";
    vbs << "      LogStep \"expected target missing \" & target(i)\r\n";
    vbs << QStringLiteral("      ShowOtaFail \"备份前目标文件不存在：\" & relPath(i) & \"，请查看日志后手动打开上位机。\"\r\n");
    vbs << "      quitCode = 2\r\n";
    vbs << "      Exit For\r\n";
    vbs << "    End If\r\n";
    vbs << "    okMove = False\r\n";
    vbs << "    For retry = 1 To 12\r\n";
    vbs << "      If TryMoveFile(target(i), backup(i)) Then\r\n";
    vbs << "        okMove = True\r\n";
    vbs << "        Exit For\r\n";
    vbs << "      End If\r\n";
    vbs << "      If retry = 5 Or retry = 10 Then\r\n";
    vbs << "        LogStep \"file \" & relPath(i) & \" backup retry=\" & retry & \" err=\" & lastErr\r\n";
    vbs << "        KillHostProcesses\r\n";
    vbs << "      End If\r\n";
    vbs << "      WScript.Sleep 100\r\n";
    vbs << "    Next\r\n";
    vbs << "    If Not okMove Then\r\n";
    vbs << "      LogStep \"move target to backup FAILED \" & target(i) & \" err=\" & lastErr\r\n";
    vbs << QStringLiteral("      ShowOtaFail \"无法备份文件：\" & relPath(i) & \"，请查看日志后手动打开上位机。\"\r\n");
    vbs << "      quitCode = 2\r\n";
    vbs << "      Exit For\r\n";
    vbs << "    End If\r\n";
    vbs << "    LogStep \"file \" & relPath(i) & \" backup ok retry=\" & retry\r\n";
    vbs << "  Else\r\n";
    vbs << "    LogStep \"file \" & relPath(i) & \" new file, no backup\"\r\n";
    vbs << "  End If\r\n";
    vbs << "  If Not TryEnsureDir(fso.GetParentFolderName(target(i))) Then\r\n";
    vbs << "    LogStep \"ensure target dir FAILED \" & target(i) & \" err=\" & lastErr\r\n";
    vbs << "    RollbackAll\r\n";
    vbs << QStringLiteral("    ShowOtaFail \"无法创建目标目录：\" & relPath(i) & \"，已回退并尝试重新打开。\"\r\n");
    vbs << "    quitCode = 3\r\n";
    vbs << "    Exit For\r\n";
    vbs << "  End If\r\n";
    vbs << "  If Not fso.FileExists(staging(i)) Then\r\n";
    vbs << "    LogStep \"staging missing \" & staging(i)\r\n";
    vbs << "    RollbackAll\r\n";
    vbs << QStringLiteral("    ShowOtaFail \"升级包缺少文件：\" & relPath(i) & \"，已回退并尝试重新打开。\"\r\n");
    vbs << "    quitCode = 3\r\n";
    vbs << "    Exit For\r\n";
    vbs << "  End If\r\n";
    vbs << "  okMove = False\r\n";
    vbs << "  For retry = 1 To 10\r\n";
    vbs << "    If TryMoveFile(staging(i), target(i)) Then\r\n";
    vbs << "      okMove = True\r\n";
    vbs << "      Exit For\r\n";
    vbs << "    End If\r\n";
    vbs << "    If retry = 1 Or retry = 5 Or retry = 10 Then\r\n";
    vbs << "      LogStep \"file \" & relPath(i) & \" replace retry=\" & retry & \" err=\" & lastErr\r\n";
    vbs << "    End If\r\n";
    vbs << "    WScript.Sleep 80\r\n";
    vbs << "  Next\r\n";
    vbs << "  If Not okMove Then\r\n";
    vbs << "    LogStep \"move staging to target FAILED \" & target(i) & \" err=\" & lastErr\r\n";
    vbs << "    RollbackAll\r\n";
    vbs << QStringLiteral("    ShowOtaFail \"替换文件失败：\" & relPath(i) & \"，已回退并尝试重新打开。\"\r\n");
    vbs << "    quitCode = 3\r\n";
    vbs << "    Exit For\r\n";
    vbs << "  End If\r\n";
    vbs << "  LogStep \"file \" & relPath(i) & \" replace ok retry=\" & retry\r\n";
    vbs << "Next\r\n";
    vbs << "If quitCode = 0 Then\r\n";
    vbs << "  LogStep \"all files replaced, launching exe\"\r\n";
    vbs << "  If Not LaunchHostExe Then\r\n";
    vbs << "    LogStep \"exe launch failed err=\" & lastErr\r\n";
    vbs << "  Else\r\n";
    vbs << "    LogStep \"exe launch requested\"\r\n";
    vbs << "  End If\r\n";
    vbs << "  alive = WaitProcessAlive\r\n";
    vbs << "  If alive Then\r\n";
    vbs << "    LogStep \"health check ok, keep backup for manual rollback\"\r\n";
    vbs << "  Else\r\n";
    vbs << "    LogStep \"health check failed, rolling back\"\r\n";
    vbs << "    RollbackAll\r\n";
    vbs << "    LaunchHostExe\r\n";
    vbs << "    alive = WaitProcessAlive\r\n";
    vbs << "    If alive And Not rollbackFail Then\r\n";
    vbs << QStringLiteral("      ShowOtaFail \"新版本无法启动，已回退到上一版本。\"\r\n");
    vbs << "      quitCode = 4\r\n";
    vbs << "    Else\r\n";
    vbs << QStringLiteral("      ShowOtaFail \"新版本无法启动，回退后仍打不开，请查看日志并手动启动。\"\r\n");
    vbs << "      quitCode = 5\r\n";
    vbs << "    End If\r\n";
    vbs << "  End If\r\n";
    vbs << "End If\r\n";
    vbs << "LogStep \"vbs finished quit=\" & quitCode\r\n";
    vbs << "If Not (logf Is Nothing) Then logf.Close\r\n";
    vbs << "If fso.FileExists(vbsPath) Then TryDeleteFile vbsPath\r\n";
    vbs << "WScript.Quit quitCode\r\n";
}

static void appendRestoreBackupFiles(QTextStream& vbs) {
    vbs << "Sub RestoreBackupFiles\r\n";
    vbs << "  Dim i, okMove, retry, canMove\r\n";
    vbs << "  restoreFail = False\r\n";
    vbs << "  For i = 0 To nFiles - 1\r\n";
    vbs << "    If existed(i) Then\r\n";
    vbs << "      If fso.FileExists(backup(i)) Then\r\n";
    vbs << "        If Not TryEnsureDir(fso.GetParentFolderName(target(i))) Then\r\n";
    vbs << "          restoreFail = True\r\n";
    vbs << "          logf.WriteLine \"restore ensureDir failed: \" & target(i) & \" err=\" & lastErr\r\n";
    vbs << "        Else\r\n";
    vbs << "          canMove = True\r\n";
    vbs << "          If fso.FileExists(target(i)) Then\r\n";
    vbs << "            If Not TryDeleteFile(target(i)) Then\r\n";
    vbs << "              restoreFail = True\r\n";
    vbs << "              canMove = False\r\n";
    vbs << "              logf.WriteLine \"restore delete failed: \" & target(i) & \" err=\" & lastErr\r\n";
    vbs << "            End If\r\n";
    vbs << "          End If\r\n";
    vbs << "          If canMove Then\r\n";
    vbs << "            okMove = False\r\n";
    vbs << "            For retry = 1 To 40\r\n";
    vbs << "              If TryMoveFile(backup(i), target(i)) Then\r\n";
    vbs << "                okMove = True\r\n";
    vbs << "                Exit For\r\n";
    vbs << "              End If\r\n";
    vbs << "              KillHostProcesses\r\n";
    vbs << "              WScript.Sleep 150\r\n";
    vbs << "            Next\r\n";
    vbs << "            If okMove Then\r\n";
    vbs << "              logf.WriteLine \"restored: \" & target(i)\r\n";
    vbs << "            Else\r\n";
    vbs << "              restoreFail = True\r\n";
    vbs << "              logf.WriteLine \"restore failed: \" & target(i) & \" err=\" & lastErr\r\n";
    vbs << "            End If\r\n";
    vbs << "          End If\r\n";
    vbs << "        End If\r\n";
    vbs << "      Else\r\n";
    vbs << "        restoreFail = True\r\n";
    vbs << "        logf.WriteLine \"restore missing backup: \" & backup(i)\r\n";
    vbs << "      End If\r\n";
    vbs << "    Else\r\n";
    vbs << "      If fso.FileExists(target(i)) Then\r\n";
    vbs << "        If Not TryDeleteFile(target(i)) Then\r\n";
    vbs << "          restoreFail = True\r\n";
    vbs << "          logf.WriteLine \"restore delete new file failed: \" & target(i) & \" err=\" & lastErr\r\n";
    vbs << "        Else\r\n";
    vbs << "          logf.WriteLine \"removed new file: \" & target(i)\r\n";
    vbs << "        End If\r\n";
    vbs << "      End If\r\n";
    vbs << "    End If\r\n";
    vbs << "  Next\r\n";
    vbs << "End Sub\r\n";
}

bool HostOtaVbs::writeReplaceScript(const QString& vbsFileName,
                                    const QString& appDir,
                                    const QString& exeName,
                                    const QString& logPath,
                                    const QString& stagingDir,
                                    const QString& backupDir,
                                    const QList<ReplaceEntry>& files) {
    if (files.isEmpty()) {
        return false;
    }
    QFile vbsFile(vbsFileName);
    if (!vbsFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    const QString oldPid = QString::number(QCoreApplication::applicationPid());
    const QString killPidsCsv = collectMatchingProcessPidsCsv(exeName);
    const int n = files.size();
    QTextStream vbs(&vbsFile);
    vbs << "Set sh = CreateObject(\"WScript.Shell\")\r\n";
    vbs << "Set fso = CreateObject(\"Scripting.FileSystemObject\")\r\n";
    vbs << "sh.CurrentDirectory = \"" << vbsQuote(appDir) << "\"\r\n";
    vbs << "Dim pid, exeName, logPath, backupDir, vbsPath, killPids, excludePids\r\n";
    vbs << "pid = \"" << oldPid << "\"\r\n";
    vbs << "exeName = \"" << vbsQuote(exeName) << "\"\r\n";
    vbs << "logPath = \"" << vbsQuote(logPath) << "\"\r\n";
    vbs << "backupDir = \"" << vbsQuote(backupDir) << "\"\r\n";
    vbs << "vbsPath = \"" << vbsQuote(vbsFileName) << "\"\r\n";
    vbs << "killPids = \"" << killPidsCsv << "\"\r\n";
    vbs << "excludePids = killPids\r\n";
    appendTryFileOps(vbs);
    appendLogStep(vbs);
    vbs << "Dim nFiles\r\n";
    vbs << "nFiles = " << n << "\r\n";
    vbs << "If Not TryEnsureDir(fso.GetParentFolderName(logPath)) Then\r\n";
    vbs << QStringLiteral("  MsgBox \"无法创建升级日志目录\", 16, \"上位机升级失败\"\r\n");
    vbs << "  WScript.Quit 1\r\n";
    vbs << "End If\r\n";
    vbs << "On Error Resume Next\r\n";
    vbs << "Err.Clear\r\n";
    vbs << "Set logf = fso.OpenTextFile(logPath, 8, True)\r\n";
    vbs << "lastErr = Err.Number\r\n";
    vbs << "On Error GoTo 0\r\n";
    vbs << "If lastErr <> 0 Then\r\n";
    vbs << QStringLiteral("  MsgBox \"无法写入升级日志\", 16, \"上位机升级失败\"\r\n");
    vbs << "  WScript.Quit 1\r\n";
    vbs << "End If\r\n";
    vbs << "LogStep \"vbs start pid=\" & pid & \" nFiles=\" & nFiles\r\n";
    vbs << "Dim staging(), target(), backup(), relPath(), existed()\r\n";
    vbs << "ReDim staging(nFiles - 1)\r\n";
    vbs << "ReDim target(nFiles - 1)\r\n";
    vbs << "ReDim backup(nFiles - 1)\r\n";
    vbs << "ReDim relPath(nFiles - 1)\r\n";
    vbs << "ReDim existed(nFiles - 1)\r\n";
    for (int i = 0; i < n; ++i) {
        const ReplaceEntry& spec = files.at(i);
        vbs << "relPath(" << i << ") = \"" << vbsQuote(spec.relPath) << "\"\r\n";
        vbs << "staging(" << i << ") = \"" << vbsQuote(QDir(stagingDir).filePath(spec.relPath)) << "\"\r\n";
        vbs << "target(" << i << ") = \"" << vbsQuote(QDir(appDir).filePath(spec.relPath)) << "\"\r\n";
        vbs << "backup(" << i << ") = \"" << vbsQuote(QDir(backupDir).filePath(spec.relPath)) << "\"\r\n";
        vbs << "existed(" << i << ") = " << (spec.existedBefore ? "True" : "False") << "\r\n";
    }
    appendReplaceBody(vbs);
    return true;
}

bool HostOtaVbs::writeRollbackScript(const QString& vbsFileName,
                                     const QString& appDir,
                                     const QString& exeName,
                                     const QString& logPath,
                                     const QList<BackupFile>& files) {
    if (files.isEmpty()) {
        return false;
    }
    QFile vbsFile(vbsFileName);
    if (!vbsFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    const QString oldPid = QString::number(QCoreApplication::applicationPid());
    const QString killPidsCsv = collectMatchingProcessPidsCsv(exeName);
    const int n = files.size();
    QTextStream vbs(&vbsFile);
    vbs << "Set sh = CreateObject(\"WScript.Shell\")\r\n";
    vbs << "Set fso = CreateObject(\"Scripting.FileSystemObject\")\r\n";
    vbs << "sh.CurrentDirectory = \"" << vbsQuote(appDir) << "\"\r\n";
    vbs << "Dim pid, exeName, logPath, vbsPath, killPids, excludePids\r\n";
    vbs << "pid = \"" << oldPid << "\"\r\n";
    vbs << "exeName = \"" << vbsQuote(exeName) << "\"\r\n";
    vbs << "logPath = \"" << vbsQuote(logPath) << "\"\r\n";
    vbs << "vbsPath = \"" << vbsQuote(vbsFileName) << "\"\r\n";
    vbs << "killPids = \"" << killPidsCsv << "\"\r\n";
    vbs << "excludePids = killPids\r\n";
    appendTryFileOps(vbs);
    appendLogStep(vbs);
    vbs << "If Not TryEnsureDir(fso.GetParentFolderName(logPath)) Then\r\n";
    vbs << QStringLiteral("  MsgBox \"无法创建回退日志目录\", 16, \"上位机升级失败\"\r\n");
    vbs << "  WScript.Quit 1\r\n";
    vbs << "End If\r\n";
    vbs << "On Error Resume Next\r\n";
    vbs << "Err.Clear\r\n";
    vbs << "Set logf = fso.CreateTextFile(logPath, True)\r\n";
    vbs << "lastErr = Err.Number\r\n";
    vbs << "On Error GoTo 0\r\n";
    vbs << "If lastErr <> 0 Then\r\n";
    vbs << QStringLiteral("  MsgBox \"无法写入回退日志\", 16, \"上位机升级失败\"\r\n");
    vbs << "  WScript.Quit 1\r\n";
    vbs << "End If\r\n";
    vbs << "logf.WriteLine Now & \" OTA rollback start pid=\" & pid\r\n";
    vbs << "Dim nFiles\r\n";
    vbs << "nFiles = " << n << "\r\n";
    vbs << "Dim backup(), target(), existed()\r\n";
    vbs << "ReDim backup(nFiles - 1)\r\n";
    vbs << "ReDim target(nFiles - 1)\r\n";
    vbs << "ReDim existed(nFiles - 1)\r\n";
    for (int i = 0; i < n; ++i) {
        vbs << "backup(" << i << ") = \"" << vbsQuote(files.at(i).backupPath) << "\"\r\n";
        vbs << "target(" << i << ") = \"" << vbsQuote(files.at(i).targetPath) << "\"\r\n";
        vbs << "existed(" << i << ") = " << (files.at(i).existedBefore ? "True" : "False") << "\r\n";
    }
    appendCommonHelpers(vbs);
    appendRestoreBackupFiles(vbs);
    vbs << "KillHostProcesses\r\n";
    vbs << "WScript.Sleep 400\r\n";
    vbs << "excludePids = GetHostPidCsv()\r\n";
    vbs << "Dim restoreFail, alive, quitCode\r\n";
    vbs << "quitCode = 0\r\n";
    vbs << "RestoreBackupFiles\r\n";
    vbs << "If Not LaunchHostExe Then\r\n";
    vbs << "  logf.WriteLine \"launch exe failed err=\" & lastErr\r\n";
    vbs << "End If\r\n";
    vbs << "alive = WaitProcessAlive\r\n";
    vbs << "If restoreFail Or Not alive Then\r\n";
    vbs << QStringLiteral("  ShowOtaFail \"回退失败或上位机无法打开，请查看日志后手动启动。\"\r\n");
    vbs << "  quitCode = 2\r\n";
    vbs << "End If\r\n";
    vbs << "logf.WriteLine \"rollback finished quit=\" & quitCode\r\n";
    vbs << "If Not (logf Is Nothing) Then logf.Close\r\n";
    vbs << "If fso.FileExists(vbsPath) Then TryDeleteFile vbsPath\r\n";
    vbs << "WScript.Quit quitCode\r\n";
    return true;
}

bool HostOtaVbs::launchAndExit(const QString& vbsFileName) {
    const QString appDir = QCoreApplication::applicationDirPath();
    bool launched = false;
#ifdef Q_OS_WIN
    {
        // 不能带 //B，否则失败 MsgBox 会被吞掉
        const QString params = QStringLiteral("//Nologo \"%1\"").arg(QDir::toNativeSeparators(vbsFileName));
        const std::wstring paramsW = params.toStdWString();
        const std::wstring dirW = QDir::toNativeSeparators(appDir).toStdWString();
        SHELLEXECUTEINFOW sei;
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"open";
        sei.lpFile = L"wscript.exe";
        sei.lpParameters = paramsW.c_str();
        sei.lpDirectory = dirW.c_str();
        sei.nShow = SW_HIDE;
        if (ShellExecuteExW(&sei)) {
            launched = true;
            if (sei.hProcess)
                CloseHandle(sei.hProcess);
        }
    }
#endif
    if (!launched) {
        QProcess helper;
        helper.setProgram(QStringLiteral("wscript.exe"));
        helper.setArguments({QStringLiteral("//Nologo"), QDir::toNativeSeparators(vbsFileName)});
        helper.setWorkingDirectory(appDir);
#ifdef Q_OS_WIN
        helper.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
            args->flags |= CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS;
            args->flags |= CREATE_BREAKAWAY_FROM_JOB;
            args->inheritHandles = false;
        });
#endif
        if (!helper.startDetached())
            return false;
    }

#ifdef Q_OS_WIN
    // 给 wscript 一点启动时间后立刻硬杀本进程，跳过 Qt 缓慢退场
    Sleep(80);
    TerminateProcess(GetCurrentProcess(), 0);
#endif
    QTimer::singleShot(0, []() { QCoreApplication::quit(); });
    return true;
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
