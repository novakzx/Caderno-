#include "services/Preferencias.h"

#include <QSettings>

namespace Preferencias {

namespace {
const char *kChaveBloqueio = "seguranca/bloqueioMin";
const char *kChaveAtualizacao = "atualizacao/verificar";
const char *kChaveUltima = "atualizacao/ultima";

bool valido(int minutos)
{
    for (int m : kBloqueiosPossiveis)
        if (m == minutos)
            return true;
    return false;
}
}  // namespace

int bloqueioEmMinutos()
{
    const int minutos = QSettings().value(QLatin1String(kChaveBloqueio), 0).toInt();
    return valido(minutos) ? minutos : 0;
}

void definirBloqueioEmMinutos(int minutos)
{
    QSettings().setValue(QLatin1String(kChaveBloqueio), valido(minutos) ? minutos : 0);
}

Atualizacao verificarAtualizacoes()
{
    const QVariant v = QSettings().value(QLatin1String(kChaveAtualizacao));
    if (!v.isValid())
        return Atualizacao::NaoPerguntado;
    return v.toBool() ? Atualizacao::Sim : Atualizacao::Nao;
}

void definirVerificarAtualizacoes(bool verificar)
{
    QSettings().setValue(QLatin1String(kChaveAtualizacao), verificar);
}

QDateTime ultimaVerificacaoDeAtualizacao()
{
    return QSettings().value(QLatin1String(kChaveUltima)).toDateTime();
}

void marcarVerificacaoDeAtualizacao()
{
    QSettings().setValue(QLatin1String(kChaveUltima), QDateTime::currentDateTime());
}

}  // namespace Preferencias
