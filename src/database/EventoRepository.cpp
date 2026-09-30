#include "database/EventoRepository.h"

#include "database/SqlUtil.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {

// Datas dos eventos podem vir com hora ("2026-10-02 08:00"): só os 10 primeiros
// caracteres (a data) importam para o calendário.
const char *SELECT_BASE =
    "SELECT e.id, COALESCE(e.turma_id, 0), e.titulo, e.tipo, substr(e.data_inicio, 1, 10), "
    "substr(e.data_fim, 1, 10), e.descricao, COALESCE(t.nome, ''), COALESCE(t.cor, '#4C8BF5') "
    "FROM eventos e LEFT JOIN turmas t ON t.id = e.turma_id ";

Evento lerEvento(const QSqlQuery &q)
{
    Evento e;
    e.id = q.value(0).toInt();
    e.turmaId = q.value(1).toInt();
    e.titulo = q.value(2).toString();
    e.tipo = q.value(3).toString();
    e.dataInicio = lerData(q.value(4));
    e.dataFim = lerData(q.value(5));
    e.descricao = q.value(6).toString();
    e.turmaNome = q.value(7).toString();
    e.turmaCor = q.value(8).toString();
    return e;
}

}  // namespace

QList<Evento> EventoRepository::listarPeriodo(const QDate &de, const QDate &ate)
{
    QList<Evento> resultado;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "%1WHERE substr(e.data_inicio, 1, 10) <= :ate "
        "AND COALESCE(NULLIF(substr(e.data_fim, 1, 10), ''), substr(e.data_inicio, 1, 10)) >= :de "
        "ORDER BY e.data_inicio, e.id")
                  .arg(QLatin1String(SELECT_BASE)));
    ligar(q, QStringLiteral(":de"), de.toString(Qt::ISODate));
    ligar(q, QStringLiteral(":ate"), ate.toString(Qt::ISODate));

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next())
        resultado.append(lerEvento(q));
    return resultado;
}

std::optional<Evento> EventoRepository::buscar(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("%1WHERE e.id = :id").arg(QLatin1String(SELECT_BASE)));
    ligar(q, QStringLiteral(":id"), id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return std::nullopt;
    }
    if (!q.next())
        return std::nullopt;
    return lerEvento(q);
}

int EventoRepository::inserir(const Evento &e)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO eventos (turma_id, titulo, tipo, data_inicio, data_fim, descricao) "
        "VALUES (:turma, :titulo, :tipo, :inicio, :fim, :descricao)"));
    ligar(q, QStringLiteral(":turma"), nuloSeZero(e.turmaId));
    ligar(q, QStringLiteral(":titulo"), e.titulo);
    ligar(q, QStringLiteral(":tipo"), e.tipo);
    ligar(q, QStringLiteral(":inicio"), e.dataInicio.toString(Qt::ISODate));
    ligar(q, QStringLiteral(":fim"), dataOuNulo(e.dataFim));
    ligar(q, QStringLiteral(":descricao"), e.descricao);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return 0;
    }
    return q.lastInsertId().toInt();
}

bool EventoRepository::atualizar(const Evento &e)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "UPDATE eventos SET turma_id = :turma, titulo = :titulo, tipo = :tipo, data_inicio = :inicio, "
        "data_fim = :fim, descricao = :descricao WHERE id = :id"));
    ligar(q, QStringLiteral(":turma"), nuloSeZero(e.turmaId));
    ligar(q, QStringLiteral(":titulo"), e.titulo);
    ligar(q, QStringLiteral(":tipo"), e.tipo);
    ligar(q, QStringLiteral(":inicio"), e.dataInicio.toString(Qt::ISODate));
    ligar(q, QStringLiteral(":fim"), dataOuNulo(e.dataFim));
    ligar(q, QStringLiteral(":descricao"), e.descricao);
    ligar(q, QStringLiteral(":id"), e.id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

bool EventoRepository::remover(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM eventos WHERE id = :id"));
    ligar(q, QStringLiteral(":id"), id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

QString EventoRepository::motivoDeDiaSemAula(const QDate &dia)
{
    for (const Evento &e : listarDoDia(dia)) {
        if (e.tipo == QLatin1String("feriado") || e.tipo == QLatin1String("recesso"))
            return e.titulo;
    }
    return QString();
}
