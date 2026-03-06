#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QFile>
#include <iostream>

#include "clicontroller.h"
#include "db/kirjanpito.h"
#include "sqlite/sqlitemodel.h"

CLIController::CLIController(QObject *parent) : QObject(parent)
{
}

void CLIController::execute(const QString &command, const QString &data)
{
    QStringList parts = command.split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        exitWithError(400, "Command is empty");
        return;
    }

    KpKysely::Metodi method = KpKysely::GET;
    QString path;

    if (parts.size() >= 2) {
        method = parseMethod(parts[0]);
        path = parts[1];
    } else {
        path = parts[0];
    }

    if (!path.startsWith('/')) {
        path.prepend('/');
    }

    QVariant payload;
    if (!data.isEmpty()) {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError) {
            exitWithError(400, "JSON parse error: " + error.errorString());
            return;
        }
        payload = doc.toVariant();
    }

    KpKysely *kysely = kp()->yhteysModel() ? kp()->yhteysModel()->kysely(path, method) : nullptr;
    if (!kysely) {
        exitWithError(404, "Could not create query for path: " + path);
        return;
    }

    connect(kysely, &KpKysely::vastaus, this, &CLIController::handleResponse);
    connect(kysely, &KpKysely::lisaysVastaus, this, &CLIController::handleAdditionResponse);
    connect(kysely, &KpKysely::virhe, this, &CLIController::handleError);

    kysely->kysy(payload);
}

KpKysely::Metodi CLIController::parseMethod(const QString &methodStr)
{
    QString m = methodStr.toUpper();
    if (m == "GET") return KpKysely::GET;
    if (m == "POST") return KpKysely::POST;
    if (m == "PATCH") return KpKysely::PATCH;
    if (m == "PUT") return KpKysely::PUT;
    if (m == "DELETE") return KpKysely::DELETE;
    return KpKysely::GET;
}

void CLIController::handleResponse(QVariant *reply)
{
    printResult(*reply);
    QCoreApplication::exit(0);
}

void CLIController::handleAdditionResponse(const QVariant &reply, int id)
{
    QVariantMap result;
    result["id"] = id;
    result["data"] = reply;
    printResult(result);
    QCoreApplication::exit(0);
}

void CLIController::handleError(int code, const QString &explanation)
{
    exitWithError(code, explanation);
}

void CLIController::printResult(const QVariant &result)
{
    QJsonDocument doc = QJsonDocument::fromVariant(result);
    std::cout << doc.toJson(QJsonDocument::Indented).constData() << std::endl;
}

void CLIController::exitWithError(int code, const QString &message)
{
    QJsonObject error;
    error["code"] = code;
    error["message"] = message;
    QJsonDocument doc(error);
    std::cerr << doc.toJson(QJsonDocument::Indented).constData() << std::endl;
    QCoreApplication::exit(1);
}
