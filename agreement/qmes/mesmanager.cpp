#include "mesmanager.h"

QMesManager::QMesManager()
{
    MesSystems.push_back(&JjMes);
    MesSystems.push_back(&XwdMes);
    MesSystems.push_back(&LxMes);
    MesSystems.push_back(&HqMes);

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

    connect(&LxMes, &Qmes::sendMesState, this, &QMesManager::handleMesState);
    connect(&LxMes, &Qmes::operateMesSucess, this, &QMesManager::handleMesSucess);
    connect(&LxMes, &Qmes::operateMesError, this, &QMesManager::handleMesError);
    connect(&LxMes, &Qmes::sendMesTestvalue, this, &QMesManager::handleMesTestvalue);
}

void QMesManager::handleMesState(int state)
{
    emit MesState(state);
}
void QMesManager::handleMesSucess(const int mechines)
{
    emit MesSucess(mechines);
}
void QMesManager::handleMesError(const int mechines, QString resultMsg)
{
    emit MesError(mechines, resultMsg);
}
void QMesManager::handleMesTestvalue(const int mechines, QString resultMsg)
{
    emit MesTestvalue(mechines, resultMsg);
}

void QMesManager::loginAll(MesPacketData pack)
{
    for (auto mes : MesSystems)
    {
        mes->LogIn(pack);
    }
}

void QMesManager::GetTestDataAll(MesPacketData pack)
{
    for (auto mes : MesSystems)
    {
        mes->GetTestData(pack);
    }
}
void QMesManager::ProcessInspectionAll(MesPacketData pack)
{
    for (auto mes : MesSystems)
    {
        mes->ProcessInspection(pack);
    }
}

void QMesManager::TestPassAll(MesPacketData pack)
{
    for (auto mes : MesSystems)
    {
        mes->TestPass(pack);
    }
}
