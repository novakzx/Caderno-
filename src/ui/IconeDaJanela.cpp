#include "ui/IconeDaJanela.h"

#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>

#include <QHash>
#endif

namespace IconeDaJanela {

#ifdef Q_OS_WIN
namespace {

// Nome do recurso em resources/app.rc.in. Os ícones carregados valem até o programa fechar
// (guardados por tamanho para não repetir o carregamento a cada login).
HICON carregar(int lado)
{
    static QHash<int, HICON> guardados;
    if (const auto it = guardados.constFind(lado); it != guardados.constEnd())
        return it.value();
    HICON icone = static_cast<HICON>(
        LoadImageW(GetModuleHandleW(nullptr), L"IDI_ICON1", IMAGE_ICON, lado, lado, LR_DEFAULTCOLOR));
    guardados.insert(lado, icone);  // também guarda a falha (nullptr): sem recurso, o Qt segue com o ícone dele
    return icone;
}

}  // namespace
#endif

void aplicar(QWidget *janela)
{
#ifdef Q_OS_WIN
    if (!janela)
        return;
    const HWND h = reinterpret_cast<HWND>(janela->winId());
    if (!h)
        return;
    const int pequeno = qMax(16, GetSystemMetrics(SM_CXSMICON));  // a tela já informa o tamanho conforme a escala
    const int grande = qMax(32, GetSystemMetrics(SM_CXICON));
    if (HICON p = carregar(pequeno))
        SendMessageW(h, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(p));
    if (HICON g = carregar(grande))
        SendMessageW(h, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(g));
#else
    Q_UNUSED(janela);
#endif
}

}  // namespace IconeDaJanela
