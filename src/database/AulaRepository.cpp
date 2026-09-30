#include "database/AulaRepository.h"

#include "database/SqlUtil.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {

const char *SELECT_BASE =
    "SELECT a.id, a.turma_id, a.data, a.tema, a.objetivos, a.materiais, a.observacoes, t.nome "
    "FROM aulas a JOIN turmas t ON t.id = a.turma_id ";

Aula lerAula(const QSqlQuery &q)
{
    Aula a;
    a.id = q.value(0).toInt();
    a.turmaId = q.value(1).toInt();
    a.data = lerData(q.value(2));
    a.tema = q.value(3).toString();
    a.objetivos = q.value(4).toString();
    a.materiais = q.value(5).toString();
    a.observacoes = q.value(6).toString();
    a.turmaNome = q.value(7).toString();
    return a;
}

}  // namespace

QList<Aula> AulaRepository::listar(int turmaId)
{
    QList<Aula> resultado;
    QSqlQuery q;
    q.prepare(QStringLiteral("%1WHERE t.arquivada = 0 AND (:turma = 0 OR a.turma_id = :turma) "
                             "ORDER BY a.data DESC, a.id DESC")
                  .arg(QLatin1String(SELECT_BASE)));
    ligar(q, QStringLiteral(":turma"), turmaId);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next())
        resultado.append(lerAula(q));
    return resultado;
}

std::optional<Aula> AulaRepository::buscar(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("%1WHERE a.id = :id").arg(QLatin1String(SELECT_BASE)));
    ligar(q, QStringLiteral(":id"), id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return std::nullopt;
    }
    if (!q.next())
        return std::nullopt;
    return lerAula(q);
}

int AulaRepository::inserir(const Aula &a)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO aulas (turma_id, data, tema, objetivos, materiais, observacoes) "
        "VALUES (:turma, :data, :tema, :obj, :mat, :obs)"));
    ligar(q, QStringLiteral(":turma"), a.turmaId);
    ligar(q, QStringLiteral(":data"), a.data.toString(Qt::ISODate));
    ligar(q, QStringLiteral(":tema"), a.tema);
    ligar(q, QStringLiteral(":obj"), a.objetivos);
    ligar(q, QStringLiteral(":mat"), a.materiais);
    ligar(q, QStringLiteral(":obs"), a.observacoes);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return 0;
    }
    return q.lastInsertId().toInt();
}

bool AulaRepository::atualizar(const Aula &a)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "UPDATE aulas SET turma_id = :turma, data = :data, tema = :tema, objetivos = :obj, "
        "materiais = :mat, observacoes = :obs WHERE id = :id"));
    ligar(q, QStringLiteral(":turma"), a.turmaId);
    ligar(q, QStringLiteral(":data"), a.data.toString(Qt::ISODate));
    ligar(q, QStringLiteral(":tema"), a.tema);
    ligar(q, QStringLiteral(":obj"), a.objetivos);
    ligar(q, QStringLiteral(":mat"), a.materiais);
    ligar(q, QStringLiteral(":obs"), a.observacoes);
    ligar(q, QStringLiteral(":id"), a.id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

bool AulaRepository::remover(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM aulas WHERE id = :id"));
    ligar(q, QStringLiteral(":id"), id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}
