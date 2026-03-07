#ifndef CLICONTROLLER_H
#define CLICONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QJsonDocument>
#include <QJsonObject>

#include "db/kirjanpito.h"
#include "sqlite/sqlitekysely.h"

class CLIController : public QObject
{
    Q_OBJECT
public:
    explicit CLIController(QObject *parent = nullptr);

    /**
     * @brief Executes a command and exits the application
     * @param command Command string (e.g. "GET tilit" or "POST tositteet")
     * @param data JSON data for POST/PUT/PATCH
     */
    void execute(const QString& command, const QString& data = QString());

private slots:
    void handleResponse(QVariant* reply);
    void handleAdditionResponse(const QVariant& reply, int id);
    void handleError(int code, const QString& explanation);
    void doExecute();

private:
    void printResult(const QVariant& result);
    void exitWithError(int code, const QString& message);

    KpKysely::Metodi parseMethod(const QString& methodStr);

    QString command_;
    QString data_;
};

#endif // CLICONTROLLER_H
