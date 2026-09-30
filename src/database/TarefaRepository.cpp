#include "database/TarefaRepository.h"

#include "database/SqlUtil.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {

const char *SELECT_BASE =
    "SELECT t.id, COALESCE(t.turma_id, 0), t.titulo, t.descricao, t.data_entrega, t.concluida, "
    "t.prioridade, COALESCE(u.nome, '') FROM tarefas t LEFT JOIN turmas u ON u.id = t.turma_id ";

Tarefa lerTarefa(const QSqlQuery &q)
{
    Tarefa t;
    t.id = q.value(0).toInt();
    t.turmaId = q.value(1).toInt();
    t.titulo = q.value(2).toString();
    t.descricao = q.value(3).toString();
    t.dataEntrega = lerData(q.value(4));
    t.concluida = q.value(5).toInt() != 0;
    t.prioridade = q.value(6).toInt();
    t.turmaNome = q.value(7).toString();
    return t;
}

}  // namespace

QList<Tarefa> TarefaRepository::consultar(const QString &where,
                                          const QList<QPair<QString, QVariant>> &parametros,
                                          const QString &ordem, int limite)
{
    QList<Tarefa> resultado;
    QSqlQuery q;
    q.prepare(QStringLiteral("%1WHERE %2 ORDER BY %3 LIMIT :limite")
                  .arg(QLatin1String(SELECT_BASE), where, ordem));
    for (const auto &p : parametros)
        ligar(q, p.first, p.second);
    ligar(q, QStringLiteral(":limite"), limite);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next())
        resultado.append(lerTarefa(q));
    return resultado;
}

QList<Tarefa> TarefaRepository::listar(Filtro filtro, int turmaId, int limite)
{
    QString situacao;
    switch (filtro) {
    case Filtro::Pendentes:  situacao = QStringLiteral("t.concluida = 0"); break;
    case Filtro::Concluidas: situacao = QStringLiteral("t.concluida = 1"); break;
    case Filtro::Todas:      situacao = QStringLiteral("1 = 1"); break;
    }
    // Com prazo primeiro (as atrasadas têm as datas mais antigas), sem prazo por último.
    const QString ordem = QStringLiteral("(t.data_entrega IS NULL), t.data_entrega, t.prioridade DESC, t.id");
    return consultar(QStringLiteral("%1 AND (:turma = 0 OR t.turma_id = :turma)").arg(situacao),
                     {{QStringLiteral(":turma"), turmaId}}, ordem, limite);
}

QList<Tarefa> TarefaRepository::listarComPrazo(const QDate &de, const QDate &ate)
{
    return consultar(QStringLiteral("t.data_entrega IS NOT NULL AND substr(t.data_entrega, 1, 10) BETWEEN :de AND :ate"),
                     {{QStringLiteral(":de"), de.toString(Qt::ISODate)},
                      {QStringLiteral(":ate"), ate.toString(Qt::ISODate)}},
                     QStringLiteral("t.data_entrega, t.id"), 1000);
}

std::optional<Tarefa> TarefaRepository::buscar(int id)
{
    const QList<Tarefa> r = consultar(QStringLiteral("t.id = :id"), {{QStringLiteral(":id"), id}},
                                      QStringLiteral("t.id"), 1);
    if (r.isEmpty())
        return std::nullopt;
    return r.first();
}

int TarefaRepository::inserir(const Tarefa &t)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO tarefas (turma_id, titulo, descricao, data_entrega, concluida, prioridade) "
        "VALUES (:turma, :titulo, :descricao, :entrega, :concluida, :prioridade)"));
    ligar(q, QStringLiteral(":turma"), nuloSeZero(t.turmaId));  // 0 = sem turma -> NULL
    ligar(q, QStringLiteral(":titulo"), t.titulo);
    ligar(q, QStringLiteral(":descricao"), t.descricao);
    ligar(q, QStringLiteral(":entrega"), dataOuNulo(t.dataEntrega));
    ligar(q, QStringLiteral(":concluida"), t.concluida ? 1 : 0);
    ligar(q, QStringLiteral(":prioridade"), t.prioridade);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return 0;
    }
    return q.lastInsertId().toInt();
}

bool TarefaRepository::atualizar(const Tarefa &t)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "UPDATE tarefas SET turma_id = :turma, titulo = :titulo, descricao = :descricao, "
        "data_entrega = :entrega, concluida = :concluida, prioridade = :prioridade WHERE id = :id"));
    ligar(q, QStringLiteral(":turma"), nuloSeZero(t.turmaId));
    ligar(q, QStringLiteral(":titulo"), t.titulo);
    ligar(q, QStringLiteral(":descricao"), t.descricao);
    ligar(q, QStringLiteral(":entrega"), dataOuNulo(t.dataEntrega));
    ligar(q, QStringLiteral(":concluida"), t.concluida ? 1 : 0);
    ligar(q, QStringLiteral(":prioridade"), t.prioridade);
    ligar(q, QStringLiteral(":id"), t.id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

bool TarefaRepository::marcarConcluida(int id, bool concluida)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE tarefas SET concluida = :c WHERE id = :id"));
    ligar(q, QStringLiteral(":c"), concluida ? 1 : 0);
    ligar(q, QStringLiteral(":id"), id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

bool TarefaRepository::remover(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM tarefas WHERE id = :id"));
    ligar(q, QStringLiteral(":id"), id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}
