#ifndef PRODUCTLICENSE_H
#define PRODUCTLICENSE_H
#include <QtCore>

struct LicensePair {
    QString product_name;
    QString device_key;
    QString device_secret;
};

class ProductLicense {
public:
    LicensePair getNextOtaDevice();
    void loadOtaDeviceKeys(const QString& filePath);
    void printOtaDeviceKeys();                    // 调试用
    QList<QMap<QString, QString>> otaDeviceList;  // 存储 OTA 设备密钥信息
    int otaDeviceIndex = 0;
    int counter = 0;
    int testcounter = 0;
    int usacounter = 0;
    int prousacounter = 0;

    static ProductLicense& getInstance() {
        static ProductLicense instance;
        return instance;
    }

    LicensePair getLicense();
    LicensePair getTestLicense();
    LicensePair getUsaLicense();
    LicensePair getProUsaLicense();
    LicensePair getCloudLicense(const QString& environment, const QString& productKey, const QString& deviceIdMac);

    static ProductLicense& getTestInstance() {
        static ProductLicense Testinstance;
        return Testinstance;
    }

private:
    ProductLicense();
    ProductLicense(const ProductLicense&) = delete;
    ProductLicense& operator=(const ProductLicense&) = delete;

    int max;
    int testmax;
    int usacountermax;
    int usaprocountermax;
};

#endif  // PRODUCTLICENSE_H
