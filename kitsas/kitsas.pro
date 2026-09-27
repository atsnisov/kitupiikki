# Kitsas (c) Arto Hyvättinen ja Kitsas Oy
# GPL License
#
# Tässä tiedostossa on tarvittavien kirjastojen
# määrittelyt. Muokkaa tarvittaessa tiedoston paikallista
# kopiota

# CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT



linux {
    DEFINES += USE_ZIPLIB
    LIBS += -lzip
}

windows {
 #   DEFINES += USE_ZIPLIB
 #   LIBS += -lzip
    # openjpeg vain, jos se löytyy kehittäjän koneelta (lähdekoodi ei käytä sitä)
    OPENJPEG_DIR = $$PWD/../../../../openjpeg-v2.5.0-windows-x64/openjpeg-v2.5.0-windows-x64
    exists($$OPENJPEG_DIR/lib) {
        LIBS += -L$$OPENJPEG_DIR/lib/ -lopenjp2
        INCLUDEPATH += $$OPENJPEG_DIR/include
        DEPENDPATH += $$OPENJPEG_DIR/include
    }
    LIBS += -lbcrypt


}

macx {
    QMAKE_XCODE_ATTRIBUTE[ALWAYS_SEARCH_USER_PATHS] = NO
    
    DEFINES += USE_ZIPLIB
    INCLUDEPATH += /opt/homebrew/opt/libzip/include
    LIBS += -L/opt/homebrew/opt/libzip/lib -lzip
    
    INCLUDEPATH += /opt/homebrew/opt/openssl/include
    LIBS += -L/opt/homebrew/opt/openssl/lib -lcrypto -lssl
}


# Otetaan mukaan tiedostot, joissa määritellään
# Kitsaan käyttämät Qt-määrittelyt sekä
# lähdekoodit.

include(kitsas.pri)
include(sources.pri) 
include(pdftuonti.pri)



