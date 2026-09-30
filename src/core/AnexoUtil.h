#pragma once

#include <QFileInfo>
#include <QSet>
#include <QString>

// Arquivos que o programa NÃO abre pelo sistema: executáveis e scripts.
// Anexos existem para apresentações, PDFs e planilhas; abrir um .exe ou .bat
// "pelo programa padrão" executaria código, então isso é recusado por segurança.
inline bool ehArquivoExecutavel(const QString &caminho)
{
    static const QSet<QString> proibidas = {
        QStringLiteral("exe"), QStringLiteral("bat"), QStringLiteral("cmd"), QStringLiteral("com"),
        QStringLiteral("msi"), QStringLiteral("ps1"), QStringLiteral("psm1"), QStringLiteral("vbs"),
        QStringLiteral("vbe"), QStringLiteral("js"), QStringLiteral("jse"), QStringLiteral("wsf"),
        QStringLiteral("wsh"), QStringLiteral("scr"), QStringLiteral("pif"), QStringLiteral("lnk"),
        QStringLiteral("jar"), QStringLiteral("reg"), QStringLiteral("hta"), QStringLiteral("cpl"),
        QStringLiteral("msc"), QStringLiteral("app"), QStringLiteral("command"), QStringLiteral("sh"),
        QStringLiteral("appimage"), QStringLiteral("desktop"), QStringLiteral("dmg"), QStringLiteral("pkg"),
    };
    return proibidas.contains(QFileInfo(caminho).suffix().toLower());
}
