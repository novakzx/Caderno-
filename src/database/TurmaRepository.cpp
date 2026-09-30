#include "database/TurmaRepository.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {

// Converte a linha atual da consulta em Turma.
// A ordem das colunas precisa bater com COLUNAS abaixo.
Turma lerTurma(const QSqlQuery &q)
{
    Turma t;
    t.id = q.value(0).toInt();
    t.nome = q.value(1).toString();
    t.disciplina = q.value(2).toString();
    t.anoLetivo = q.value(3).toInt();
    t.periodo = q.value(4).toString();
    t.sala = q.value(5).toString();
    t.cor = q.value(6).toString();
    t.arquivada = q.value(7).toInt() != 0;
    t.totalAlunos = q.value(8).toInt();
    return t;
}

// Alunos ativos são contados por subconsulta (campo calculado).
const char *COLUNAS =
    "t.id, t.nome, t.disciplina, t.ano_letivo, t.periodo, t.sala, t.cor, t.arquivada, "
    "(SELECT COUNT(*) FROM alunos a WHERE a.turma_id = t.id AND a.ativo = 1)";

}  // namespace

QList<Turma> TurmaRepository::listar(bool incluirArquivadas)
{
    QList<Turma> resultado;
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT %1 FROM turmas t "
                             "WHERE (:todas = 1 OR t.arquivada = 0) "
                             "ORDER BY t.ano_letivo DESC, t.nome COLLATE NOCASE")
                  .arg(QLatin1String(COLUNAS)));
    q.bindValue(QStringLiteral(":todas"), incluirArquivadas ? 1 : 0);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next())
        resultado.append(lerTurma(q));
    return resultado;
}

std::optional<Turma> TurmaRepository::buscar(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT %1 FROM turmas t WHERE t.id = :id")
                  .arg(QLatin1String(COLUNAS)));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return std::nullopt;
    }
    if (!q.next())
        return std::nullopt;
    return lerTurma(q);
}

int TurmaRepository::inserir(const Turma &t)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO turmas (nome, disciplina, ano_letivo, periodo, sala, cor, arquivada) "
        "VALUES (:nome, :disciplina, :ano, :periodo, :sala, :cor, :arquivada)"));
    q.bindValue(QStringLiteral(":nome"), t.nome);
    q.bindValue(QStringLiteral(":disciplina"), t.disciplina);
    q.bindValue(QStringLiteral(":ano"), t.anoLetivo);
    q.bindValue(QStringLiteral(":periodo"), t.periodo);
    q.bindValue(QStringLiteral(":sala"), t.sala);
    q.bindValue(QStringLiteral(":cor"), t.cor);
    q.bindValue(QStringLiteral(":arquivada"), t.arquivada ? 1 : 0);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return 0;
    }
    return q.lastInsertId().toInt();
}

bool TurmaRepository::atualizar(const Turma &t)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "UPDATE turmas SET nome = :nome, disciplina = :disciplina, ano_letivo = :ano, "
        "periodo = :periodo, sala = :sala, cor = :cor, arquivada = :arquivada "
        "WHERE id = :id"));
    q.bindValue(QStringLiteral(":nome"), t.nome);
    q.bindValue(QStringLiteral(":disciplina"), t.disciplina);
    q.bindValue(QStringLiteral(":ano"), t.anoLetivo);
    q.bindValue(QStringLiteral(":periodo"), t.periodo);
    q.bindValue(QStringLiteral(":sala"), t.sala);
    q.bindValue(QStringLiteral(":cor"), t.cor);
    q.bindValue(QStringLiteral(":arquivada"), t.arquivada ? 1 : 0);
    q.bindValue(QStringLiteral(":id"), t.id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

bool TurmaRepository::remover(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM turmas WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}
