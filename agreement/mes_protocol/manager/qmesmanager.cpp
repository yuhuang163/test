#include "qmesmanager.h"

#include "test_record_store.h"

QMesManager::QMesManager() {
    MesSystems.push_back(&BydMes);
    MesSystems.push_back(&JjMes);
    MesSystems.push_back(&XwdMes);
    MesSystems.push_back(&LxMes);
    MesSystems.push_back(&HqMes);
    MesSystems.push_back(&HzMes);
    MesSystems.push_back(&WksMes);
    MesSystems.push_back(&YdmMes);

    connect(&BydMes, &Qmes::sendMesState, this, &QMesManager::handleMesState);
    connect(&BydMes, &Qmes::operateMesSucess, this, &QMesManager::handleMesSucess);
    connect(&BydMes, &Qmes::operateMesError, this, &QMesManager::handleMesError);
    connect(&BydMes, &Qmes::sendMesTestvalue, this, &QMesManager::handleMesTestvalue);

    connect(&JjMes, &Qmes::sendMesState, this, &QMesManager::handleMesState);
    connect(&JjMes, &Qmes::operateMesSucess, this, &QMesManager::handleMesSucess);
    connect(&JjMes, &Qmes::operateMesError, this, &QMesManager::handleMesError);
    connect(&JjMes, &Qmes::sendMesTestvalue, this, &QMesManager::handleMesTestvalue);

    connect(&XwdMes, &Qmes::sendMesState, this, &QMesManager::handleMesState);
    connect(&XwdMes, &Qmes::operateMesSucess, this, &QMesManager::handleMesSucess);
    connect(&XwdMes, &Qmes::operateMesError, this, &QMesManager::handleMesError);
    connect(&XwdMes, &Qmes::sendMesTestvalue, this, &QMesManager::handleMesTestvalue);

    connect(&HqMes, &Qmes::sendMesState, this, &QMesManager::handleMesState);
    connect(&HqMes, &Qmes::operateMesSucess, this, &QMesManager::handleMesSucess);
    connect(&HqMes, &Qmes::operateMesError, this, &QMesManager::handleMesError);
    connect(&HqMes, &Qmes::sendMesTestvalue, this, &QMesManager::handleMesTestvalue);

    connect(&HzMes, &Qmes::sendMesState, this, &QMesManager::handleMesState);
    connect(&HzMes, &Qmes::operateMesSucess, this, &QMesManager::handleMesSucess);
    connect(&HzMes, &Qmes::operateMesError, this, &QMesManager::handleMesError);
    connect(&HzMes, &Qmes::sendMesTestvalue, this, &QMesManager::handleMesTestvalue);

    connect(&LxMes, &Qmes::sendMesState, this, &QMesManager::handleMesState);
    connect(&LxMes, &Qmes::operateMesSucess, this, &QMesManager::handleMesSucess);
    connect(&LxMes, &Qmes::operateMesError, this, &QMesManager::handleMesError);
    connect(&LxMes, &Qmes::sendMesTestvalue, this, &QMesManager::handleMesTestvalue);

    connect(&WksMes, &Qmes::sendMesState, this, &QMesManager::handleMesState);
    connect(&WksMes, &Qmes::operateMesSucess, this, &QMesManager::handleMesSucess);
    connect(&WksMes, &Qmes::operateMesError, this, &QMesManager::handleMesError);
    connect(&WksMes, &Qmes::sendMesTestvalue, this, &QMesManager::handleMesTestvalue);

    connect(&YdmMes, &Qmes::sendMesState, this, &QMesManager::handleMesState);
    connect(&YdmMes, &Qmes::operateMesSucess, this, &QMesManager::handleMesSucess);
    connect(&YdmMes, &Qmes::operateMesError, this, &QMesManager::handleMesError);
    connect(&YdmMes, &Qmes::sendMesTestvalue, this, &QMesManager::handleMesTestvalue);
}

void QMesManager::handleMesState(int state) {
    emit MesState(state);
}
void QMesManager::handleMesSucess(const int mechines) {
    emit MesSucess(mechines);
}
void QMesManager::handleMesError(const int mechines, QString resultMsg) {
    emit MesError(mechines, resultMsg);
}
void QMesManager::handleMesTestvalue(const int mechines, QString resultMsg) {
    emit MesTestvalue(mechines, resultMsg);
}

void QMesManager::loginAll(MesPacketData pack) {
    for (auto mes : MesSystems) {
        mes->LogIn(pack);
    }
}

void QMesManager::GetTestDataAll(MesPacketData pack) {
    for (auto mes : MesSystems) {
        mes->GetTestData(pack);
    }
}
void QMesManager::ProcessInspectionAll(MesPacketData pack) {
    for (auto mes : MesSystems) {
        mes->ProcessInspection(pack);
    }
}

void QMesManager::TestPassAll(MesPacketData pack) {
    // 与 MES 过站同入口写本地库，避免 box_base 信号槽未触发时漏记
    TestRecordStore::instance().saveOnTestPass(pack);
    for (auto mes : MesSystems) {
        mes->TestPass(pack);
    }
}
