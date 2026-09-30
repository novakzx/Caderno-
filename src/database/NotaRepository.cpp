#include "database/NotaRepository.h"
#include "database/SqlUtil.h"

#include <QSqlError>
#include <QSqlQuery>

QHash<qint64, double> NotaRepository::listarPorTurma(int turmaId)
{
    QHash<qint64, double> notas;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT n.avaliacao_id, n.aluno_id, n.valor FROM notas n "
        "JOIN avaliacoes a ON a.id = n.avaliacao_id "
        "WHERE a.turma_id = :turma AND n.valor IS NOT NULL"));
    ligar(q, QStringLiteral(":turma"), turmaId);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return notas;
    }
    while (q.next())
        notas.insert(chave(q.value(0).toInt(), q.value(1).toInt()), q.value(2).toDouble());
    return notas;
}

bool NotaRepository::salvar(int avaliacaoId, int alunoId, std::optional<double> valor)
{
    QSqlQuery q;
    if (!valor) {
        // Célula esvaziada: remove a linha em vez de guardar NULL.
        q.prepare(QStringLiteral("DELETE FROM notas WHERE avaliacao_id = :av AND aluno_id = :al"));
        ligar(q, QStringLiteral(":av"), avaliacaoId);
        ligar(q, QStringLiteral(":al"), alunoId);
    } else {
        // UPSERT: usa a restrição UNIQUE (avaliacao_id, aluno_id) da tabela.
        q.prepare(QStringLiteral(
            "INSERT INTO notas (avaliacao_id, aluno_id, valor) VALUES (:av, :al, :valor) "
            "ON CONFLICT (avaliacao_id, aluno_id) DO UPDATE SET valor = excluded.valor"));
        ligar(q, QStringLiteral(":av"), avaliacaoId);
        ligar(q, QStringLiteral(":al"), alunoId);
        ligar(q, QStringLiteral(":valor"), *valor);
    }

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

int NotaRepository::contarAcima(int avaliacaoId, double limite)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM notas WHERE avaliacao_id = :av AND valor > :lim"));
    ligar(q, QStringLiteral(":av"), avaliacaoId);
    ligar(q, QStringLiteral(":lim"), limite);

    if (!q.exec() || !q.next()) {
        m_erro = q.lastError().text();
        return 0;
    }
    return q.value(0).toInt();
}
