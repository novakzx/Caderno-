#include "services/LembreteService.h"

#include "database/AgendaRepository.h"
#include "database/TarefaRepository.h"

#include <QCryptographicHash>
#include <QStringList>

namespace {

QString quando(int dias)
{
    return dias == 0 ? QStringLiteral("hoje") : QStringLiteral("amanhã");
}

// Texto do usuário (título de prova) não entra na chave; só um resumo dele, curto e fixo.
QString resumo(const QString &texto)
{
    return QString::fromLatin1(QCryptographicHash::hash(texto.toUtf8(), QCryptographicHash::Sha1).toHex().left(10));
}

}  // namespace

QList<Lembrete> LembreteService::pendentes(const QDateTime &agora, const LembreteUtil::Config &config)
{
    QList<Lembrete> lista;
    if (!config.ativos)
        return lista;

    const QDate hoje = agora.date();
    const QString dia = hoje.toString(Qt::ISODate);
    const int minutoAgora = agora.time().hour() * 60 + agora.time().minute();

    // --- Aulas que começam em instantes ---
    for (const auto &aula : m_agenda.aulasDoDia(hoje)) {
        const Horario &h = aula.horario;
        const int minutoInicio = h.inicio.hour() * 60 + h.inicio.minute();
        const int faltam = LembreteUtil::minutosParaAvisarAula(minutoAgora, minutoInicio, config.antecedenciaAula);
        if (faltam < 0)
            continue;

        QString nome = h.turmaNome;
        if (!h.turmaDisciplina.isEmpty())
            nome += QStringLiteral(" — ") + h.turmaDisciplina;
        QStringList detalhes{h.inicio.toString(QStringLiteral("HH:mm"))};
        if (!h.sala.isEmpty())
            detalhes << QStringLiteral("sala %1").arg(h.sala);
        if (!aula.tema.isEmpty())
            detalhes << aula.tema;

        Lembrete l;
        l.chave = QStringLiteral("aula:%1:%2").arg(h.id).arg(dia);
        l.titulo = faltam == 1 ? QStringLiteral("Aula em 1 minuto") : QStringLiteral("Aula em %1 minutos").arg(faltam);
        l.texto = nome + QStringLiteral(" · ") + detalhes.join(QStringLiteral(" · "));
        lista << l;
    }

    // --- Tarefas e provas de hoje e de amanhã ---
    if (config.prazos) {
        for (const Tarefa &t : m_tarefas.listarPendentes(500)) {
            if (!t.dataEntrega.isValid())
                continue;
            const int dias = static_cast<int>(hoje.daysTo(t.dataEntrega));
            if (!LembreteUtil::deveAvisarPrazo(dias, agora.time().hour()))
                continue;
            Lembrete l;
            l.chave = QStringLiteral("tarefa:%1:%2").arg(t.id).arg(dia);
            l.titulo = QStringLiteral("Tarefa para %1").arg(quando(dias));
            l.texto = t.turmaNome.isEmpty() ? t.titulo : QStringLiteral("%1 · %2").arg(t.titulo, t.turmaNome);
            lista << l;
        }

        for (const auto &prova : m_agenda.provasProximas(hoje, 1)) {
            const int dias = static_cast<int>(hoje.daysTo(prova.data));
            if (!LembreteUtil::deveAvisarPrazo(dias, agora.time().hour()))
                continue;
            Lembrete l;
            l.chave = QStringLiteral("prova:%1:%2:%3").arg(resumo(prova.titulo + prova.turmaNome), prova.data.toString(Qt::ISODate), dia);
            l.titulo = QStringLiteral("Prova %1").arg(quando(dias));
            l.texto = prova.turmaNome.isEmpty() ? prova.titulo : QStringLiteral("%1 · %2").arg(prova.titulo, prova.turmaNome);
            lista << l;
        }
    }
    return lista;
}
