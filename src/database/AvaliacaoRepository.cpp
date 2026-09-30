#include "database/AvaliacaoRepository.h"
#include "database/SqlUtil.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

const char *COLUNAS = "id, turma_id, nome, tipo, peso, nota_maxima, data, periodo, ordem";

Avaliacao lerAvaliacao(const QSqlQuery &q)
{
    Avaliacao a;
    a.id = q.value(0).toInt();
    a.turmaId = q.value(1).toInt();
    a.nome = q.value(2).toString();
    a.tipo = q.value(3).toString();
    a.peso = q.value(4).toDouble();
    a.notaMaxima = q.value(5).toDouble();
    a.data = QDate::fromString(q.value(6).toString(), Qt::ISODate);
    a.periodo = q.value(7).toInt();
    a.ordem = q.value(8).toInt();
    return a;
}

QVariant dataParaBanco(const QDate &d)
{
    return d.isValid() ? QVariant(d.toString(Qt::ISODate)) : QVariant();
}

}  // namespace

QList<Avaliacao> AvaliacaoRepository::listarPorTurma(int turmaId, int periodo)
{
    QList<Avaliacao> resultado;
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT %1 FROM avaliacoes WHERE turma_id = :turma "
                             "AND (:periodo = 0 OR periodo = :periodo) "
                             "ORDER BY periodo, ordem, id")
                  .arg(QLatin1String(COLUNAS)));
    ligar(q, QStringLiteral(":turma"), turmaId);
    ligar(q, QStringLiteral(":periodo"), periodo);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next())
        resultado.append(lerAvaliacao(q));
    return resultado;
}

std::optional<Avaliacao> AvaliacaoRepository::buscar(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT %1 FROM avaliacoes WHERE id = :id").arg(QLatin1String(COLUNAS)));
    ligar(q, QStringLiteral(":id"), id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return std::nullopt;
    }
    if (!q.next())
        return std::nullopt;
    return lerAvaliacao(q);
}

int AvaliacaoRepository::inserir(const Avaliacao &a)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO avaliacoes (turma_id, nome, tipo, peso, nota_maxima, data, periodo, ordem) "
        "VALUES (:turma, :nome, :tipo, :peso, :max, :data, :periodo, "
        "(SELECT COALESCE(MAX(ordem), 0) + 1 FROM avaliacoes "
        " WHERE turma_id = :turma AND periodo = :periodo))"));
    ligar(q, QStringLiteral(":turma"), a.turmaId);
    ligar(q, QStringLiteral(":nome"), a.nome);
    ligar(q, QStringLiteral(":tipo"), a.tipo);
    ligar(q, QStringLiteral(":peso"), a.peso);
    ligar(q, QStringLiteral(":max"), a.notaMaxima);
    ligar(q, QStringLiteral(":data"), dataParaBanco(a.data));
    ligar(q, QStringLiteral(":periodo"), a.periodo);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return 0;
    }
    return q.lastInsertId().toInt();
}

bool AvaliacaoRepository::atualizar(const Avaliacao &a)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "UPDATE avaliacoes SET nome = :nome, tipo = :tipo, peso = :peso, nota_maxima = :max, "
        "data = :data, periodo = :periodo WHERE id = :id"));
    ligar(q, QStringLiteral(":nome"), a.nome);
    ligar(q, QStringLiteral(":tipo"), a.tipo);
    ligar(q, QStringLiteral(":peso"), a.peso);
    ligar(q, QStringLiteral(":max"), a.notaMaxima);
    ligar(q, QStringLiteral(":data"), dataParaBanco(a.data));
    ligar(q, QStringLiteral(":periodo"), a.periodo);
    ligar(q, QStringLiteral(":id"), a.id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

bool AvaliacaoRepository::remover(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM avaliacoes WHERE id = :id"));
    ligar(q, QStringLiteral(":id"), id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

bool AvaliacaoRepository::mover(int id, int delta)
{
    const auto atual = buscar(id);
    if (!atual || delta == 0)
        return false;

    // Ids do mesmo período, na ordem atual.
    QList<int> ids;
    for (const Avaliacao &a : listarPorTurma(atual->turmaId, atual->periodo))
        ids.append(a.id);

    const int pos = ids.indexOf(id);
    const int destino = pos + (delta < 0 ? -1 : 1);
    if (pos < 0 || destino < 0 || destino >= ids.size())
        return false;  // já está na ponta
    ids.swapItemsAt(pos, destino);

    // Regrava a ordem de todos (1..n): evita problemas quando várias têm a mesma ordem.
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE avaliacoes SET ordem = :ordem WHERE id = :id"));
    for (int i = 0; i < ids.size(); ++i) {
        ligar(q, QStringLiteral(":ordem"), i + 1);
        ligar(q, QStringLiteral(":id"), ids.at(i));
        if (!q.exec()) {
            m_erro = q.lastError().text();
            db.rollback();
            return false;
        }
    }
    return db.commit();
}
