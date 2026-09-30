#include "database/FrequenciaRepository.h"

#include "database/SqlUtil.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {

RegistroFrequencia lerRegistro(const QSqlQuery &q)
{
    RegistroFrequencia r;
    r.alunoId = q.value(0).toInt();
    r.data = lerData(q.value(1));
    const QString s = q.value(2).toString();
    r.situacao = s.isEmpty() ? QLatin1Char('P') : s.at(0);
    r.justificativa = q.value(3).toString();
    return r;
}

}  // namespace

QHash<int, RegistroFrequencia> FrequenciaRepository::doDia(int turmaId, const QDate &data)
{
    QHash<int, RegistroFrequencia> resultado;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT f.aluno_id, f.data, f.situacao, f.justificativa FROM frequencia f "
        "JOIN alunos a ON a.id = f.aluno_id WHERE a.turma_id = :turma AND f.data = :data"));
    ligar(q, QStringLiteral(":turma"), turmaId);
    ligar(q, QStringLiteral(":data"), data.toString(Qt::ISODate));

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next()) {
        const RegistroFrequencia r = lerRegistro(q);
        resultado.insert(r.alunoId, r);
    }
    return resultado;
}

bool FrequenciaRepository::salvar(int alunoId, const QDate &data, QChar situacao, const QString &justificativa)
{
    QSqlQuery q;
    // UPSERT usando a restrição UNIQUE (aluno_id, data) da tabela.
    q.prepare(QStringLiteral(
        "INSERT INTO frequencia (aluno_id, data, situacao, justificativa) "
        "VALUES (:aluno, :data, :sit, :just) "
        "ON CONFLICT (aluno_id, data) DO UPDATE SET situacao = excluded.situacao, "
        "justificativa = excluded.justificativa"));
    ligar(q, QStringLiteral(":aluno"), alunoId);
    ligar(q, QStringLiteral(":data"), data.toString(Qt::ISODate));
    ligar(q, QStringLiteral(":sit"), QString(situacao));
    ligar(q, QStringLiteral(":just"), justificativa);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

bool FrequenciaRepository::remover(int alunoId, const QDate &data)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM frequencia WHERE aluno_id = :aluno AND data = :data"));
    ligar(q, QStringLiteral(":aluno"), alunoId);
    ligar(q, QStringLiteral(":data"), data.toString(Qt::ISODate));

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

bool FrequenciaRepository::marcarRestantes(int turmaId, const QDate &data, QChar situacao)
{
    QSqlQuery q;
    // INSERT OR IGNORE: a restrição UNIQUE (aluno_id, data) pula quem já tem registro.
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO frequencia (aluno_id, data, situacao) "
        "SELECT id, :data, :sit FROM alunos WHERE turma_id = :turma AND ativo = 1"));
    ligar(q, QStringLiteral(":data"), data.toString(Qt::ISODate));
    ligar(q, QStringLiteral(":sit"), QString(situacao));
    ligar(q, QStringLiteral(":turma"), turmaId);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

QHash<int, ResumoFrequencia> FrequenciaRepository::resumoPorAluno(int turmaId, const QDate &de, const QDate &ate)
{
    QHash<int, ResumoFrequencia> resultado;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT f.aluno_id, "
        "SUM(CASE WHEN f.situacao = 'P' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN f.situacao = 'F' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN f.situacao = 'J' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN f.situacao = 'A' THEN 1 ELSE 0 END) "
        "FROM frequencia f JOIN alunos a ON a.id = f.aluno_id "
        "WHERE a.turma_id = :turma AND (:de = '' OR f.data >= :de) AND (:ate = '' OR f.data <= :ate) "
        "GROUP BY f.aluno_id"));
    ligar(q, QStringLiteral(":turma"), turmaId);
    ligar(q, QStringLiteral(":de"), de.isValid() ? de.toString(Qt::ISODate) : QString());
    ligar(q, QStringLiteral(":ate"), ate.isValid() ? ate.toString(Qt::ISODate) : QString());

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next()) {
        ResumoFrequencia r;
        r.presencas = q.value(1).toInt();
        r.faltas = q.value(2).toInt();
        r.justificadas = q.value(3).toInt();
        r.atrasos = q.value(4).toInt();
        resultado.insert(q.value(0).toInt(), r);
    }
    return resultado;
}

QList<RegistroFrequencia> FrequenciaRepository::doMes(int turmaId, int ano, int mes)
{
    QList<RegistroFrequencia> resultado;
    const QDate inicio(ano, mes, 1);
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT f.aluno_id, f.data, f.situacao, f.justificativa FROM frequencia f "
        "JOIN alunos a ON a.id = f.aluno_id "
        "WHERE a.turma_id = :turma AND f.data >= :de AND f.data <= :ate ORDER BY f.data"));
    ligar(q, QStringLiteral(":turma"), turmaId);
    ligar(q, QStringLiteral(":de"), inicio.toString(Qt::ISODate));
    ligar(q, QStringLiteral(":ate"), inicio.addMonths(1).addDays(-1).toString(Qt::ISODate));

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next())
        resultado.append(lerRegistro(q));
    return resultado;
}

QList<RegistroFrequencia> FrequenciaRepository::doAluno(int alunoId)
{
    QList<RegistroFrequencia> resultado;
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT aluno_id, data, situacao, justificativa FROM frequencia "
                             "WHERE aluno_id = :aluno ORDER BY data DESC"));
    ligar(q, QStringLiteral(":aluno"), alunoId);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next())
        resultado.append(lerRegistro(q));
    return resultado;
}
