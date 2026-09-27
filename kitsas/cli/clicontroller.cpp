#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QFile>
#include <iostream>
#include <string>
#include <QCommandLineParser>

#ifdef Q_OS_WIN
#include <conio.h>
#include <cwchar>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include "clicontroller.h"
#include "db/kirjanpito.h"
#include "sqlite/sqlitemodel.h"
#include "pilvi/pilvimodel.h"
#include "aloitussivu/loginservice.h"
#include <QTimer>
#include <QSettings>

// stdout on varattu komennon JSON-tulokselle, joten kaikki muu tulostus
// (edistyminen, kehotteet, virheilmoitukset ihmiselle) menee stderr:iin.

namespace {

// Luetaan salasana näyttämättä sitä päätteessä
std::string readPassword()
{
    std::string password;
#ifdef Q_OS_WIN
    std::wstring input;
    for (;;) {
        const wint_t c = _getwch();
        if (c == L'\r' || c == L'\n' || c == WEOF)
            break;
        if (c == 0 || c == 0xE0) {
            _getwch();      // Erikoisnäppäimen toinen koodi ohitetaan
        } else if (c == L'\b') {
            if (!input.empty())
                input.pop_back();
        } else {
            input.push_back(static_cast<wchar_t>(c));
        }
    }
    password = QString::fromStdWString(input).toStdString();
#else
    termios oldt;
    tcgetattr(STDIN_FILENO, &oldt);
    termios newt = oldt;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    std::cin >> password;
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
    return password;
}

}

CLIController::CLIController(QObject *parent) : QObject(parent)
{
}

int CLIController::run(const QString &command, const QString &data, const QString &file)
{
    // Pilvitilassa ei tarvita paikallista tiedostoa
    if (!isCloudMode()) {
        if (file.isEmpty() || !QFile::exists(file)) {
            exitWithError(404, "Bookkeeping file not found: " + file);
            return 1;
        }
        if (!kp()->sqlite()->avaaTiedosto(file)) {
            exitWithError(500, "Cannot open bookkeeping file: " + file);
            return 1;
        }
    }

    std::cerr << "Käynnistetään CLI-ohjaus..." << std::endl;
    QTimer::singleShot(0, this, [this, command, data]() {
        std::cerr << "Suoritetaan komento: " << command.toStdString() << std::endl;
        execute(command, data);
    });
    // Ellei komentoon tule vastausta, ei jäädä odottamaan ikuisesti
    QTimer::singleShot(120000, this, [this]() {
        exitWithError(504, "Timeout: no response to the command within 120 s.");
    });
    return QCoreApplication::exec();
}

bool CLIController::isCloudMode()
{
    const QStringList args = QCoreApplication::arguments();
    return args.contains("--pro") || args.contains("--api");
}

void CLIController::execute(const QString &command, const QString &data)
{
    std::cerr << "CLIController::execute käynnistyy..." << std::endl;
    command_ = command;
    data_ = data;

    if (isCloudMode()) {
        if (kp()->yhteysModel()) {
            std::cerr << "Kirjanpito on jo auki." << std::endl;
            doExecute();
        } else {
            // Katsotaan onko meillä valmis istunto
            if (kp()->settings()->contains("AuthKey")) {
                std::cerr << "Käytetään valmiiksi tallennettua istuntoavainta..." << std::endl;
                LoginService *login = new LoginService(nullptr);
                login->keyLogin();
            } else {
                // Kysytään tunnukset interaktiivisesti
                std::string email, password;
                std::cerr << "Kirjaudu Kitsas-pilveen" << std::endl;
                std::cerr << "Sähköposti: ";
                std::cin >> email;
                
                std::cerr << "Salasana: ";
                password = readPassword();
                std::cerr << std::endl;
                
                QVariantMap map;
                map.insert("email", QString::fromStdString(email));
                map.insert("password", QString::fromStdString(password));
                map.insert("requestKey", true);
                
                LoginService *login = new LoginService(nullptr);
                login->auth(map);
            }

            std::cerr << "Odotetaan kirjautumista ja kirjanpitojen latautumista..." << std::endl;
            connect(kp()->pilvi(), &PilviModel::kirjauduttu, this, [this](PilviKayttaja user) {
                if (user) {
                    std::cerr << "Käyttäjä tunnistettu: " << user.nimi().toStdString() << std::endl;
                    if (kp()->yhteysModel()) {
                        std::cerr << "Kirjanpito avautui automaattisesti." << std::endl;
                        doExecute();
                    } else if (kp()->pilvi()->rowCount() > 0) {
                        int id = kp()->pilvi()->index(0, 0).data(PilviModel::IdRooli).toInt();
                        QString nimi = kp()->pilvi()->index(0, 0).data(PilviModel::NimiRooli).toString();
                        std::cerr << "Avataan kirjanpito: " << nimi.toStdString() << " (ID: " << id << ")" << std::endl;
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
        bool ok = false;
        method = parseMethod(parts[0], &ok);
        if (!ok) {
            exitWithError(400, "Unknown method: " + parts[0]);
            return;
        }
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
        // Pilvessä kirjanpito voi olla vielä latautumassa
        if (isCloudMode()) {
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

KpKysely::Metodi CLIController::parseMethod(const QString &methodStr, bool *ok)
{
    QString m = methodStr.toUpper();
    *ok = true;
    if (m == "GET") return KpKysely::GET;
    if (m == "POST") return KpKysely::POST;
    if (m == "PATCH") return KpKysely::PATCH;
    if (m == "PUT") return KpKysely::PUT;
    if (m == "DELETE") return KpKysely::DELETE;
    *ok = false;
    return KpKysely::GET;
}

// Lisäyksessä kysely lähettää ensin vastaus-signaalin ja heti perään
// lisaysVastaus-signaalin. Siksi tulos tulostetaan vasta tapahtumasilmukan
// seuraavalla kierroksella, jotta stdout:iin tulee aina tasan yksi JSON.

void CLIController::handleResponse(QVariant *reply)
{
    result_ = *reply;
    scheduleFinish();
}

void CLIController::handleAdditionResponse(const QVariant &reply, int id)
{
    QVariantMap result;
    result["id"] = id;
    result["data"] = reply;
    result_ = result;
    scheduleFinish();
}

void CLIController::handleError(int code, const QString &explanation)
{
    exitWithError(code, explanation);
}

void CLIController::scheduleFinish()
{
    if (!finishScheduled_) {
        finishScheduled_ = true;
        QTimer::singleShot(0, this, &CLIController::finish);
    }
}

void CLIController::finish()
{
    if (finished_)
        return;
    finished_ = true;
    printResult(result_);
    QCoreApplication::exit(0);
}

void CLIController::printResult(const QVariant &result)
{
    const QJsonValue value = QJsonValue::fromVariant(result);
    QByteArray json;
    if (value.isObject()) {
        json = QJsonDocument(value.toObject()).toJson(QJsonDocument::Indented);
    } else if (value.isArray()) {
        json = QJsonDocument(value.toArray()).toJson(QJsonDocument::Indented);
    } else {
        // Yksittäinen arvo (esim. null): QJsonDocument ei tue sitä suoraan
        json = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
        json = json.mid(1, json.length() - 2) + "\n";
    }
    std::cout << json.constData() << std::flush;
}

void CLIController::exitWithError(int code, const QString &message)
{
    if (finished_)
        return;
    finished_ = true;

    QJsonObject error;
    error["code"] = code;
    error["message"] = message;
    std::cout << QJsonDocument(error).toJson(QJsonDocument::Indented).constData() << std::flush;
    std::cerr << "Virhe " << code << ": " << message.toStdString() << std::endl;
    QCoreApplication::exit(1);
}
