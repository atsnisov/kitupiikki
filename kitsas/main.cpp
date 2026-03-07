#include <QApplication>
#include <QCommandLineParser>
#include <QLocale>
#include <QTranslator>
#include <QSettings>
#include <QSplashScreen>
#include <QDir>
#include <QFont>
#include <QFontDatabase>

#include "versio.h"
#include "db/kirjanpito.h"
#include "sqlite/sqlitemodel.h"
#include "kitupiikkiikkuna.h"
#include "kieli/kielet.h"
#include "aloitussivu/tervetulodialogi.h"
#include "pilvi/pilvikayttaja.h"
#include "maaritys/ulkoasumaaritys.h"
#include "pilvi/pilvimodel.h"

#include "tools/kitsaslokimodel.h"
#include "aloitussivu/toffeelogin.h"
#include "aloitussivu/loginservice.h"
#include "cli/clicontroller.h"
#include <QTimer>
#include <iostream>
#include <termios.h>
#include <unistd.h>

#include "laskutus/laskunuusinta.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setApplicationName("Kitsas");
    a.setOrganizationName("Kitsas");
    a.setApplicationVersion(KITSAS_VERSIO);

    QCommandLineParser parser;
    parser.setApplicationDescription("Kitsas - Professional Bookkeeping");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption proOption("pro", "Run in Pro mode");
    parser.addOption(proOption);

    QCommandLineOption demoOption("demo", "Run in Demo mode");
    parser.addOption(demoOption);

    QCommandLineOption apiOption("api", "Run in API mode");
    parser.addOption(apiOption);

    QCommandLineOption commandOption("command", "Execute a CLI command", "command");
    parser.addOption(commandOption);

    QCommandLineOption dataOption("data", "JSON data for CLI command", "data");
    parser.addOption(dataOption);

    parser.addPositionalArgument("file", "Database file to open");

    parser.process(a);

    if (parser.isSet("pro") || parser.isSet("api")) {
        PilviKayttaja::asetaVersioMoodi(PilviKayttaja::PRO);
    }

    PilviModel::asetaPilviLoginOsoite(KITSAS_API);

#if defined (Q_OS_WIN) || defined (Q_OS_MACX)
    QString portableDir = QCoreApplication::applicationDirPath() + "/data";
    if (QDir(portableDir).exists()) {
        a.setProperty("portable", portableDir);
    }
#endif

    Kielet::alustaKielet(":/tr/tulkki.json");

    Kirjanpito kirjanpito;
    // Kirjanpito-olio asettaa instanssi__:n itse constructorissaan (päivitetty koodi)

    QString fonttinimi = kp()->settings()->value("Fontti").toString();
    if( !fonttinimi.isEmpty()) {
        a.setFont( QFont( fonttinimi, kp()->settings()->value("FonttiKoko").toInt()) );
    }

    if (parser.isSet("command")) {
        a.setProperty("command", true);
        
        // CLI-tilassa katsotaan pitääkö avata tiedosto
        if (!parser.positionalArguments().isEmpty() && QFile(parser.positionalArguments().value(0)).exists()) {
             kirjanpito.sqlite()->avaaTiedosto(parser.positionalArguments().value(0));
        } else {
            std::cerr << "Virhe: Tietokantatiedosto puuttuu tai sitä ei löydy." << std::endl;
            return 1;
        }

        CLIController *cli = new CLIController(&a);
        std::cout << "Käynnistetään CLI-ohjaus..." << std::endl;
        QTimer::singleShot(0, [cli, &parser]() {
            std::cout << "Suoritetaan komento: " << parser.value("command").toStdString() << std::endl;
            cli->execute(parser.value("command"), parser.value("data"));
        });
        return a.exec();
    }

    if( parser.isSet("pro") ||  PRO_VERSIO ) {
        PilviKayttaja::asetaVersioMoodi(PilviKayttaja::PRO);
        ToffeeLogin loginDlg;
        if(loginDlg.keyExec() != QDialog::Accepted) {
            return 0;
        }        
    } else if( kp()->settings()->value("ViimeksiVersiolla").toString() != a.applicationVersion()  ) {
        TervetuloDialogi tervetuloa;
        if( tervetuloa.exec() != QDialog::Accepted)
            return 0;
        kp()->settings()->setValue("ViimeksiVersiolla", a.applicationVersion());
    }
    
    a.setProperty("demo", parser.isSet("demo"));

    QSplashScreen *splash = new QSplashScreen;
    splash->setPixmap( QPixmap(":/pic/splash_" + Kielet::instanssi()->uiKieli() + ".png"));
    splash->show();

    KitupiikkiIkkuna ikkuna;
    ikkuna.show();

    // Avaa argumenttina olevan tiedostonnimen
    if( !parser.positionalArguments().isEmpty() && QFile(parser.positionalArguments().value(0)).exists())
        kirjanpito.sqlite()->avaaTiedosto( parser.positionalArguments().value(0) );

    new LaskunUusinta(&kirjanpito);

    splash->finish( &ikkuna );
    delete splash;

    return a.exec();
}
