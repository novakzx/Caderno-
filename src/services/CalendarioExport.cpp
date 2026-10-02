#include "services/CalendarioExport.h"

#include "core/IcsUtil.h"
#include "database/AgendaRepository.h"
#include "database/EventoRepository.h"
#include "database/TarefaRepository.h"

#include <QCryptographicHash>
#include <vector>

namespace {

QString rotuloDoTipo(const QString &tipo)
{
    if (tipo == QLatin1String("prova")) return QStringLiteral("Prova");
    if (tipo == QLatin1String("feriado")) return QStringLiteral("Feriado");
    if (tipo == QLatin1String("recesso")) return QStringLiteral("Recesso");
    if (tipo == QLatin1String("reuniao")) return QStringLiteral("Reunião");
    return QStringLiteral("Evento");
}

std::string utf8(const QString &texto)
{
    return texto.toUtf8().toStdString();
}

IcsUtil::Evento umDia(const QString &uid, const QString &titulo, const QString &descricao, const QDate &data)
{
    IcsUtil::Evento e;
    e.uid = utf8(uid);
    e.titulo = utf8(titulo);
    e.descricao = utf8(descricao);
    e.ano = data.year();
    e.mes = data.month();
    e.dia = data.day();
    return e;
}

QString resumoCurto(const QString &texto)
{
    return QString::fromLatin1(QCryptographicHash::hash(texto.toUtf8(), QCryptographicHash::Sha1).toHex().left(16));
}

}  // namespace

namespace CalendarioExport {

QByteArray gerarIcs(EventoRepository &eventos, TarefaRepository &tarefas, AgendaRepository &agenda, const QDate &de,
                    const QDate &ate, const QDateTime &geradoEm, int *total)
{
    std::vector<IcsUtil::Evento> lista;

    for (const Evento &e : eventos.listarPeriodo(de, ate)) {
        QString titulo = QStringLiteral("%1: %2").arg(rotuloDoTipo(e.tipo), e.titulo);
        if (!e.turmaNome.isEmpty())
            titulo += QStringLiteral(" (%1)").arg(e.turmaNome);
        IcsUtil::Evento item = umDia(QStringLiteral("evento-%1@caderno-plus").arg(e.id), titulo, e.descricao, e.dataInicio);
        if (e.dataFim.isValid() && e.dataFim > e.dataInicio) {
            item.anoFim = e.dataFim.year();
            item.mesFim = e.dataFim.month();
            item.diaFim = e.dataFim.day();
        }
        lista.push_back(item);
    }

    for (const Tarefa &t : tarefas.listarComPrazo(de, ate)) {
        if (t.concluida || !t.dataEntrega.isValid())
            continue;
        QString titulo = QStringLiteral("Tarefa: %1").arg(t.titulo);
        if (!t.turmaNome.isEmpty())
            titulo += QStringLiteral(" (%1)").arg(t.turmaNome);
        lista.push_back(umDia(QStringLiteral("tarefa-%1@caderno-plus").arg(t.id), titulo, t.descricao, t.dataEntrega));
    }

    for (const auto &a : agenda.avaliacoesDatadas(de, ate)) {
        const QString rotulo = a.tipo == QLatin1String("prova") ? QStringLiteral("Prova") : QStringLiteral("Avaliação");
        const QString titulo = QStringLiteral("%1: %2 (%3)").arg(rotulo, a.nome, a.turmaNome);
        // Avaliações não têm um id exposto aqui: o UID vem de um resumo estável de turma + nome + data.
        const QString uid = QStringLiteral("avaliacao-%1@caderno-plus")
                                .arg(resumoCurto(a.turmaNome + QLatin1Char('|') + a.nome + QLatin1Char('|') + a.data.toString(Qt::ISODate)));
        lista.push_back(umDia(uid, titulo, QString(), a.data));
    }

    if (total)
        *total = static_cast<int>(lista.size());
    const QString carimbo = geradoEm.toUTC().toString(QStringLiteral("yyyyMMdd'T'HHmmss'Z'"));
    return QByteArray::fromStdString(IcsUtil::montar(lista, "Caderno+", utf8(carimbo)));
}

}  // namespace CalendarioExport
