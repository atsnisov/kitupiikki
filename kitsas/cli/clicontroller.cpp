#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QFile>
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <QCommandLineParser>

#include "clicontroller.h"
#include "db/kirjanpito.h"
#include "sqlite/sqlitemodel.h"
#include "pilvi/pilvimodel.h"
#include "aloitussivu/loginservice.h"
#include <QTimer>
#include <QSettings>

CLIController::CLIController(QObject *parent) : QObject(parent)
{
}

void CLIController::execute(const QString &command, const QString &data)
{
    std::cout << "CLIController::execute käynnistyy..." << std::endl;
    command_ = command;
    data_ = data;

    // Tarkistetaan ollaanko pilvitilassa komentoriviparametrien perusteella
    bool cloudMode = false;
    for (const QString &arg : QCoreApplication::arguments()) {
        if (arg == "--pro" || arg == "--api") {
            cloudMode = true;
            break;
        }
    }

    if (cloudMode) {
        if (kp()->yhteysModel()) {
            std::cout << "Kirjanpito on jo auki." << std::endl;
            doExecute();
        } else {
            // Katsotaan onko meillä valmis istunto
            if (kp()->settings()->contains("AuthKey")) {
                std::cout << "Käytetään valmiiksi tallennettua istuntoavainta..." << std::endl;
                LoginService *login = new LoginService(nullptr);
                login->keyLogin();
            } else {
                // Kysytään tunnukset interaktiivisesti
                std::string email, password;
                std::cout << "Kirjaudu Kitsas-pilveen" << std::endl;
                std::cout << "Sähköposti: ";
                std::cin >> email;
                
                std::cout << "Salasana: ";
                termios oldt;
                tcgetattr(STDIN_FILENO, &oldt);
                termios newt = oldt;
                newt.c_lflag &= ~ECHO;
                tcsetattr(STDIN_FILENO, TCSANOW, &newt);
                std::cin >> password;
                tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
                std::cout << std::endl;
                
                QVariantMap map;
                map.insert("email", QString::fromStdString(email));
                map.insert("password", QString::fromStdString(password));
                map.insert("requestKey", true);
                
                LoginService *login = new LoginService(nullptr);
                login->auth(map);
            }

            std::cout << "Odotetaan kirjautumista ja kirjanpitojen latautumista..." << std::endl;
            connect(kp()->pilvi(), &PilviModel::kirjauduttu, this, [this](PilviKayttaja user) {
                if (user) {
                    std::cout << "Käyttäjä tunnistettu: " << user.nimi().toStdString() << std::endl;
                    if (kp()->yhteysModel()) {
                        std::cout << "Kirjanpito avautui automaattisesti." << std::endl;
                        doExecute();
                    } else if (kp()->pilvi()->rowCount() > 0) {
                        int id = kp()->pilvi()->index(0, 0).data(PilviModel::IdRooli).toInt();
                        QString nimi = kp()->pilvi()->index(0, 0).data(PilviModel::NimiRooli).toString();
                        std::cout << "Avataan kirjanpito: " << nimi.toStdString() << " (ID: " << id << ")" << std::endl;
                        kp()->pilvi()->avaaPilvesta(id);
                        connect(kp(), &Kirjanpito::tietokantaVaihtui, this, &CLIController::doExecute, Qt::UniqueConnection);
                    } else {
                        exitWithError(404, "Kirjautuminen onnistui, mutta yhtään kirjanpitoa ei löytynyt.");
                    }
                }
            });
            
            // Aikakatkaisu 30 sekuntia
            QTimer::singleShot(30000, [this]() {
                if (!kp()->yhteysModel()) {
                    exitWithError(401, "Cloud connection timeout. Check your credentials and 2FA status.");
                }
            });
        }
    } else {
        doExecute();
    }
}

void CLIController::doExecute()
{
    disconnect(kp(), &Kirjanpito::tietokantaVaihtui, this, &CLIController::doExecute);

    QStringList parts = command_.split(' ', Qt::SkipEmptyParts);
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
    if (!data_.isEmpty()) {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data_.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError) {
            exitWithError(400, "JSON parse error: " + error.errorString());
            return;
        }
        payload = doc.toVariant();
    }

    YhteysModel *model = kp()->yhteysModel();
    if (!model) {
        // Katsotaan taas ollaanko menossa pilveen
        bool cloudMode = false;
        for (const QString &arg : QCoreApplication::arguments()) {
            if (arg == "--pro" || arg == "--api") { cloudMode = true; break; }
        }

        if (cloudMode) {
             QTimer::singleShot(1000, this, &CLIController::doExecute);
             return;
        }
        exitWithError(404, "No active bookkeeping connection.");
        return;
    }

    KpKysely *kysely = model->kysely(path, method);
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
