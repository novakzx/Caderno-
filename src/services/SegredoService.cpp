#include "services/SegredoService.h"

#include <QByteArray>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

namespace SegredoService {

#ifdef Q_OS_WIN

namespace {

// "Entropia" opcional da DPAPI: um segredo extra fixo do programa. Um outro programa do mesmo usuário
// que chame a DPAPI sem esta constante não abre o texto.
QByteArray entropia()
{
    return QByteArrayLiteral("Caderno+/segredos/v1");
}

}  // namespace

bool disponivel()
{
    return true;
}

QString proteger(const QString &texto)
{
    if (texto.isEmpty())
        return QString();
    QByteArray bruto = texto.toUtf8();
    QByteArray chaveExtra = entropia();

    DATA_BLOB entrada{static_cast<DWORD>(bruto.size()), reinterpret_cast<BYTE *>(bruto.data())};
    DATA_BLOB extra{static_cast<DWORD>(chaveExtra.size()), reinterpret_cast<BYTE *>(chaveExtra.data())};
    DATA_BLOB saida{0, nullptr};
    const BOOL ok = CryptProtectData(&entrada, L"Caderno+", &extra, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &saida);
    bruto.fill('\0');  // não deixa o segredo em texto puro na memória à toa
    if (!ok || !saida.pbData)
        return QString();
    const QByteArray protegido(reinterpret_cast<const char *>(saida.pbData), static_cast<qsizetype>(saida.cbData));
    LocalFree(saida.pbData);
    return QString::fromLatin1(protegido.toBase64());
}

QString revelar(const QString &protegido)
{
    if (protegido.isEmpty())
        return QString();
    QByteArray cifrado = QByteArray::fromBase64(protegido.toLatin1());
    if (cifrado.isEmpty())
        return QString();
    QByteArray chaveExtra = entropia();

    DATA_BLOB entrada{static_cast<DWORD>(cifrado.size()), reinterpret_cast<BYTE *>(cifrado.data())};
    DATA_BLOB extra{static_cast<DWORD>(chaveExtra.size()), reinterpret_cast<BYTE *>(chaveExtra.data())};
    DATA_BLOB saida{0, nullptr};
    if (!CryptUnprotectData(&entrada, nullptr, &extra, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &saida) || !saida.pbData)
        return QString();
    QByteArray aberto(reinterpret_cast<const char *>(saida.pbData), static_cast<qsizetype>(saida.cbData));
    SecureZeroMemory(saida.pbData, saida.cbData);
    LocalFree(saida.pbData);
    const QString texto = QString::fromUtf8(aberto);
    aberto.fill('\0');
    return texto;
}

#else  // sem DPAPI: não guarda nada

bool disponivel()
{
    return false;
}
QString proteger(const QString &)
{
    return QString();
}
QString revelar(const QString &)
{
    return QString();
}

#endif

}  // namespace SegredoService
