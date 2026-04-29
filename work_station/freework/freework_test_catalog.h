#ifndef FREEWORK_TEST_CATALOG_H
#define FREEWORK_TEST_CATALOG_H

#include <QString>
#include <QVector>

struct FreeWorkTestCatalogItem {
    int id;
    QString name;
};

QVector<FreeWorkTestCatalogItem> getFreeWorkTestCatalog();

#endif  // FREEWORK_TEST_CATALOG_H
