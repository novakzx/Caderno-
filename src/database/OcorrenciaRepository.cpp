#include "database/OcorrenciaRepository.h"

#include "core/OcorrenciaUtil.h"
#include "database/SqlUtil.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

namespace {

const char *COLUNAS = "id, aluno_id, data, tipo, texto";

Ocorrencia lerOcorrencia(const QSqlQuery &q)
{
    Ocorrencia o;
    o.id = q.value(0).toInt();
    o.alunoId = q.value(1).toInt();
    o.data = lerData(q.value(2));
    o.tipo = q.value(3).toString();
    o.texto = q.value(4).toString();
    return o;
}

// Tipo que não existe na lista cai em "outro" antes de gravar (dado sempre consistente).
QString tipoParaBanco(const QString &tipo)
{
    const std::string id = tipo.toStdString();
    return OcorrenciaUtil::idValido(id) ? tipo : QStringLiteral("outro");
}

}  // namespace

QList<Ocorrencia> OcorrenciaRepository::listarPorAluno(int alunoId)
{
    QList<Ocorrencia> resultado;
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT %1 FROM ocorrencias WHERE aluno_id = :aluno ORDER BY data DESC, id DESC")
                  .arg(QLatin1String(COLUNAS)));
    ligar(q, QStringLiteral(":aluno"), alunoId);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next())
        resultado.append(lerOcorrencia(q));
    return resultado;
}

std::optional<Ocorrencia> OcorrenciaRepository::buscar(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT %1 FROM ocorrencias WHERE id = :id").arg(QLatin1String(COLUNAS)));
    ligar(q, QStringLiteral(":id"), id);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return std::nullopt;
    }
    if (!q.next())
        return std::nullopt;
    return lerOcorrencia(q);
}

int OcorrenciaRepository::inserir(const Ocorrencia &o)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("INSERT INTO ocorrencias (aluno_id, data, tipo, texto) "
                             "VALUES (:aluno, :data, :tipo, :texto)"));
    ligar(q, QStringLiteral(":aluno"), o.alunoId);
    ligar(q, QStringLiteral(":data"), o.data.toString(Qt::ISODate));
    ligar(q, QStringLiteral(":tipo"), tipoParaBanco(o.tipo));
    ligar(q, QStringLiteral(":texto"), o.texto);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return 0;
    }
    return q.lastInsertId().toInt();
}

bool OcorrenciaRepository::atualizar(const Ocorrencia &o)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE ocorrencias SET data = :data, tipo = :tipo, texto = :texto WHERE id = :id"));
    ligar(q, QStringLiteral(":data"), o.data.toString(Qt::ISODate));
    ligar(q, QStringLiteral(":tipo"), tipoParaBanco(o.tipo));
    ligar(q, QStringLiteral(":texto"), o.texto);
    ligar(q, QStringLiteral(":id"), o.id);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

bool OcorrenciaRepository::remover(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM ocorrencias WHERE id = :id"));
    ligar(q, QStringLiteral(":id"), id);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

QHash<int, int> OcorrenciaRepository::contarNegativasPorAluno(int turmaId, const QDate &desde)
{
    QHash<int, int> resultado;

    // Os tipos "negativos" vêm de OcorrenciaUtil (uma só fonte); entram na consulta como parâmetros.
    QStringList marcadores;
    QStringList ids;
    for (const auto &tipo : OcorrenciaUtil::kTipos) {
        if (!tipo.negativa)
            continue;
        marcadores << QStringLiteral(":t%1").arg(marcadores.size());
        ids << QString::fromLatin1(tipo.id);
    }
    if (ids.isEmpty())
        return resultado;

    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT o.aluno_id, COUNT(*) FROM ocorrencias o "
                             "JOIN alunos a ON a.id = o.aluno_id "
                             "WHERE a.turma_id = :turma AND a.ativo = 1 AND o.data >= :desde AND o.tipo IN (%1) "
                             "GROUP BY o.aluno_id")
                  .arg(marcadores.join(QStringLiteral(", "))));
    ligar(q, QStringLiteral(":turma"), turmaId);
    ligar(q, QStringLiteral(":desde"), desde.toString(Qt::ISODate));
    for (int i = 0; i < ids.size(); ++i)
        ligar(q, marcadores.at(i), ids.at(i));

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next())
        resultado.insert(q.value(0).toInt(), q.value(1).toInt());
    return resultado;
}

QHash<int, int> OcorrenciaRepository::contarPorAluno(int turmaId)
{
    QHash<int, int> resultado;
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT o.aluno_id, COUNT(*) FROM ocorrencias o "
                             "JOIN alunos a ON a.id = o.aluno_id WHERE a.turma_id = :turma GROUP BY o.aluno_id"));
    ligar(q, QStringLiteral(":turma"), turmaId);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next())
        resultado.insert(q.value(0).toInt(), q.value(1).toInt());
    return resultado;
}
