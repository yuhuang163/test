#ifndef QSCPIVISASESSION_H
#define QSCPIVISASESSION_H

#include <QObject>
#include <QString>

#include "scpi_transport.h"
#include "visa_channel.h"

/** SCPI over VISA：在 VisaChannel 上组行 / 同步 query。 */
class QScpiVisaSession : public QObject, public ScpiTransport {
    Q_OBJECT

  public:
    explicit QScpiVisaSession(QObject* parent = nullptr);

    void setVisaConfig(const ScpiVisaLinkConfig& config);
    ScpiVisaLinkConfig visaConfig() const;

    bool ensureConnected();
    void closeConnection();

    ScpiLinkKind linkKind() const override {
        return ScpiLinkKind::Visa;
    }
    bool isOpen() const override;
    bool writeLine(const QString& line) override;
    bool queryLine(const QString& line, QString* responseOut) override;

  private:
    ScpiVisaLinkConfig config_;
    VisaChannel visaChannel_;
};

#endif // QSCPIVISASESSION_H
