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
    LIBS += -L$$PWD/../../../../openjpeg-v2.5.0-windows-x64/openjpeg-v2.5.0-windows-x64/lib/ -lopenjp2
    INCLUDEPATH += $$PWD/../../../../openjpeg-v2.5.0-windows-x64/openjpeg-v2.5.0-windows-x64/include
    DEPENDPATH += $$PWD/../../../../openjpeg-v2.5.0-windows-x64/openjpeg-v2.5.0-windows-x64/include
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



