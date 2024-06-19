#ifndef PRODUCTLICENSE_H
#define PRODUCTLICENSE_H
#include <QtCore>

struct LicensePair
{
    QString product_name;
    QString device_key;
    QString device_secret;
};

class ProductLicense
{
public:
    static ProductLicense &getInstance()
    {
        static ProductLicense instance;
        return instance;
    }

    LicensePair getLicense();
    LicensePair getTestLicense();

    static ProductLicense &getTestInstance()
    {
        static ProductLicense Testinstance;
        return Testinstance;
    }
private:
    ProductLicense();
    ProductLicense(const ProductLicense &) = delete;
    ProductLicense &operator=(const ProductLicense &) = delete;

    int counter;
    int testcounter;
    int max;
    int testmax;
};

#endif   // PRODUCTLICENSE_H
