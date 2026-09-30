#include "database/AlunoRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

const char *COLUNAS =
    "id, turma_id, nome, matricula, email, data_nascimento, observacoes, ativo";

Aluno lerAluno(const QSqlQuery &q)
{
    Aluno a;
    a.id = q.value(0).toInt();
    a.turmaId = q.value(1).toInt();
    a.nome = q.value(2).toString();
    a.matricula = q.value(3).toString();
    a.email = q.value(4).toString();
    a.dataNascimento = QDate::fromString(q.value(5).toString(), Qt::ISODate);
    a.observacoes = q.value(6).toString();
    a.ativo = q.value(7).toInt() != 0;
    return a;
}

// Data inválida vira NULL no banco; válida vira texto ISO (yyyy-MM-dd).
QVariant dataParaBanco(const QDate &d)
{
    return d.isValid() ? QVariant(d.toString(Qt::ISODate)) : QVariant(QMetaType(QMetaType::QString));
}

}  // namespace

QList<Aluno> AlunoRepository::listarPorTurma(int turmaId, const QString &filtro,
                                             bool incluirInativos)
{
    QList<Aluno> resultado;
    QSqlQuery q;
    q.prepare(QStringLiteral(
                  "SELECT %1 FROM alunos WHERE turma_id = :turma "
                  "AND (:inativos = 1 OR ativo = 1) "
                  "AND (:filtro = '' OR nome LIKE :pad OR matricula LIKE :pad OR email LIKE :pad) "
                  "ORDER BY nome COLLATE NOCASE")
                  .arg(QLatin1String(COLUNAS)));
    q.bindValue(QStringLiteral(":turma"), turmaId);
    q.bindValue(QStringLiteral(":inativos"), incluirInativos ? 1 : 0);
    q.bindValue(QStringLiteral(":filtro"), filtro.trimmed());
    q.bindValue(QStringLiteral(":pad"), QStringLiteral("%") + filtro.trimmed() + QStringLiteral("%"));

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next())
        resultado.append(lerAluno(q));
    return resultado;
}

std::optional<Aluno> AlunoRepository::buscar(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT %1 FROM alunos WHERE id = :id").arg(QLatin1String(COLUNAS)));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return std::nullopt;
    }
    if (!q.next())
        return std::nullopt;
    return lerAluno(q);
}

int AlunoRepository::inserir(const Aluno &a)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO alunos (turma_id, nome, matricula, email, data_nascimento, observacoes, ativo) "
        "VALUES (:turma, :nome, :matricula, :email, :nasc, :obs, :ativo)"));
    q.bindValue(QStringLiteral(":turma"), a.turmaId);
    q.bindValue(QStringLiteral(":nome"), a.nome);
    q.bindValue(QStringLiteral(":matricula"), a.matricula);
    q.bindValue(QStringLiteral(":email"), a.email);
    q.bindValue(QStringLiteral(":nasc"), dataParaBanco(a.dataNascimento));
    q.bindValue(QStringLiteral(":obs"), a.observacoes);
    q.bindValue(QStringLiteral(":ativo"), a.ativo ? 1 : 0);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return 0;
    }
    return q.lastInsertId().toInt();
}

bool AlunoRepository::atualizar(const Aluno &a)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "UPDATE alunos SET nome = :nome, matricula = :matricula, email = :email, "
        "data_nascimento = :nasc, observacoes = :obs, ativo = :ativo WHERE id = :id"));
    q.bindValue(QStringLiteral(":nome"), a.nome);
    q.bindValue(QStringLiteral(":matricula"), a.matricula);
    q.bindValue(QStringLiteral(":email"), a.email);
    q.bindValue(QStringLiteral(":nasc"), dataParaBanco(a.dataNascimento));
    q.bindValue(QStringLiteral(":obs"), a.observacoes);
    q.bindValue(QStringLiteral(":ativo"), a.ativo ? 1 : 0);
    q.bindValue(QStringLiteral(":id"), a.id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

bool AlunoRepository::remover(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM alunos WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}
