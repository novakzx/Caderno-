#include "database/AnexoRepository.h"

#include "database/SqlUtil.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {

const char *SELECT_BASE =
    "SELECT x.id, COALESCE(x.turma_id, 0), COALESCE(x.aula_id, 0), COALESCE(x.aluno_id, 0), "
    "x.nome, x.caminho, x.tipo, x.criado_em, COALESCE(a.tema, '') "
    "FROM anexos x LEFT JOIN aulas a ON a.id = x.aula_id ";

Anexo lerAnexo(const QSqlQuery &q)
{
    Anexo a;
    a.id = q.value(0).toInt();
    a.turmaId = q.value(1).toInt();
    a.aulaId = q.value(2).toInt();
    a.alunoId = q.value(3).toInt();
    a.nome = q.value(4).toString();
    a.caminho = q.value(5).toString();
    a.tipo = q.value(6).toString();
    a.criadoEm = q.value(7).toString();
    a.aulaTema = q.value(8).toString();
    return a;
}

}  // namespace

QList<Anexo> AnexoRepository::consultar(const QString &where, int valor)
{
    QList<Anexo> resultado;
    QSqlQuery q;
    q.prepare(QStringLiteral("%1WHERE %2 ORDER BY x.criado_em DESC, x.id DESC")
                  .arg(QLatin1String(SELECT_BASE), where));
    ligar(q, QStringLiteral(":valor"), valor);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next())
        resultado.append(lerAnexo(q));
    return resultado;
}

QList<Anexo> AnexoRepository::listarPorAula(int aulaId)
{
    return consultar(QStringLiteral("x.aula_id = :valor"), aulaId);
}

QList<Anexo> AnexoRepository::listarPorTurma(int turmaId)
{
    return consultar(QStringLiteral("x.turma_id = :valor"), turmaId);
}

std::optional<Anexo> AnexoRepository::buscar(int id)
{
    const QList<Anexo> r = consultar(QStringLiteral("x.id = :valor"), id);
    if (r.isEmpty())
        return std::nullopt;
    return r.first();
}

int AnexoRepository::inserir(const Anexo &a)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO anexos (turma_id, aula_id, aluno_id, nome, caminho, tipo) "
        "VALUES (:turma, :aula, :aluno, :nome, :caminho, :tipo)"));
    ligar(q, QStringLiteral(":turma"), nuloSeZero(a.turmaId));
    ligar(q, QStringLiteral(":aula"), nuloSeZero(a.aulaId));
    ligar(q, QStringLiteral(":aluno"), nuloSeZero(a.alunoId));
    ligar(q, QStringLiteral(":nome"), a.nome);
    ligar(q, QStringLiteral(":caminho"), a.caminho);
    ligar(q, QStringLiteral(":tipo"), a.tipo);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return 0;
    }
    return q.lastInsertId().toInt();
}

bool AnexoRepository::remover(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM anexos WHERE id = :id"));
    ligar(q, QStringLiteral(":id"), id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}
