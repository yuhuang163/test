#ifndef PLATFORM_TEST_CASE_CMD_CATALOGS_H
#define PLATFORM_TEST_CASE_CMD_CATALOGS_H

#include "cmd_catalog_base.h"

class DeviceCmdManifestCatalog : public CmdManifestCatalog {
  public:
    DeviceCmdManifestCatalog();
    void paramToIniGroup(QSettings& settings, int cmd, const QVariant& value) const override;

  private:
    static CmdManifestRegistry::Policy devicePolicy();
};

class UsbCameraCmdManifestCatalog : public CmdManifestCatalog {
  public:
    UsbCameraCmdManifestCatalog();
    bool paramFromIniGroup(const QSettings& settings, int cmd, QVariant& out) const override;
    void paramToIniGroup(QSettings& settings, int cmd, const QVariant& value) const override;

  private:
    static CmdManifestRegistry::Policy usbCameraPolicy();
};

class VesLightCmdManifestCatalog : public CmdManifestCatalog {
  public:
    VesLightCmdManifestCatalog();

  private:
    static CmdManifestRegistry::Policy vesLightPolicy();
};

class Asd9026aCmdManifestCatalog : public CmdManifestCatalog {
  public:
    Asd9026aCmdManifestCatalog();

  private:
    static CmdManifestRegistry::Policy asd9026aPolicy();
};

class XwdRawFixtureCmdManifestCatalog : public CmdManifestCatalog {
  public:
    XwdRawFixtureCmdManifestCatalog();

  private:
    static CmdManifestRegistry::Policy xwdPolicy();
};

class JieliBtBoxCmdManifestCatalog : public CmdManifestCatalog {
  public:
    JieliBtBoxCmdManifestCatalog();

  private:
    static CmdManifestRegistry::Policy jieliPolicy();
};

class FixturePcbaCmdManifestCatalog : public CmdManifestCatalog {
  public:
    FixturePcbaCmdManifestCatalog();
    bool paramFromIniGroup(const QSettings& settings, int cmd, QVariant& out) const override;
    void paramToIniGroup(QSettings& settings, int cmd, const QVariant& value) const override;

  private:
    static CmdManifestRegistry::Policy fixturePcbaPolicy();
};

class DongleCmdManifestCatalog : public CmdManifestCatalog {
  public:
    DongleCmdManifestCatalog();

  private:
    static CmdManifestRegistry::Policy policy();
};

class ProductSerialCmdManifestCatalog : public CmdManifestCatalog {
  public:
    ProductSerialCmdManifestCatalog();

  private:
    static CmdManifestRegistry::Policy policy();
};

class TupleCmdManifestCatalog : public CmdManifestCatalog {
  public:
    TupleCmdManifestCatalog();

  private:
    static CmdManifestRegistry::Policy policy();
};

#endif // PLATFORM_TEST_CASE_CMD_CATALOGS_H
