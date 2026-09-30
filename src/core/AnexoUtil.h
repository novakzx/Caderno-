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
        // outros que executam código ou instalam algo
        QStringLiteral("dll"), QStringLiteral("chm"), QStringLiteral("url"), QStringLiteral("inf"),
        QStringLiteral("scf"), QStringLiteral("gadget"), QStringLiteral("xll"), QStringLiteral("wsc"),
        QStringLiteral("ws"), QStringLiteral("vb"), QStringLiteral("msp"), QStringLiteral("mst"),
        QStringLiteral("appx"), QStringLiteral("msix"), QStringLiteral("appref-ms"), QStringLiteral("settingcontent-ms"),
        // imagens de disco (montam sozinhas no Windows)
        QStringLiteral("iso"), QStringLiteral("img"), QStringLiteral("vhd"), QStringLiteral("vhdx"),
        // documentos do Office com macros (podem executar código ao abrir)
        QStringLiteral("docm"), QStringLiteral("dotm"), QStringLiteral("xlsm"), QStringLiteral("xltm"),
        QStringLiteral("xlam"), QStringLiteral("pptm"), QStringLiteral("potm"), QStringLiteral("ppam"),
        QStringLiteral("ppsm"), QStringLiteral("sldm"),
    };
    return proibidas.contains(QFileInfo(caminho).suffix().toLower());
}
