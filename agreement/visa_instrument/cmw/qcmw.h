#ifndef QCMW_H
#define QCMW_H

#include <QObject>
#include <QVariant>

class Qvisa;

/** CMW100 GPRF（VISA SCPI）指令，工站经 qcmw->set/get 下发 */
enum class CmwGprfCmd {
    ClearStatus,      // set
    GenOff,           // set（OFF;*OPC?）
    GenOn,            // set（ON;*OPC?）
    ListOff,          // set
    BbModeArb,        // set
    ArbFile,          // set: QString 波形路径
    ArbRepetition,    // set: QString（如 SINGle）
    ArbCycles,        // set: int
    TxLevelDbm,       // set: double
    FrequencyMhz,     // set: int
    ManualArbTrigger, // set
    WriteLine,        // set: QString 原始 SCPI
    Identity,         // get *IDN?
    ArbFilePath,      // get SOURce:GPRF:GEN:ARB:FILE?
    ArbScount,        // get SOURce:GPRF:GEN:ARB:SCOunt?
    GenState,         // get SOURce:GPRF:GEN:STATe?
    SystemError,      // get SYSTem:ERRor?
    QueryLine,        // get: QString 原始 SCPI 查询
};

class Qcmw : public QObject {
    Q_OBJECT
  public:
    explicit Qcmw(QObject* parent = nullptr);

    void bindVisa(Qvisa* visa);

    bool set(CmwGprfCmd cmd, const QVariant& data = {});
    bool get(CmwGprfCmd cmd, const QVariant& param = {});

    QString lastQueryResponse() const;

    /** 将路径规范为 SOURce:GPRF:GEN:ARB:FILE ... 写命令 */
    static QString arbFileCommand(const QString& rawPath);

  private:
    bool writeLine(const QString& cmd);
    bool queryLine(const QString& cmd);

    Qvisa* visa_ = nullptr;
};

#endif // QCMW_H
