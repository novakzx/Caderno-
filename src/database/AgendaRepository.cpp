#include "database/AgendaRepository.h"
#include "database/SqlUtil.h"

#include <QSqlError>
#include <QSqlQuery>

QList<AgendaRepository::AulaDoDia> AgendaRepository::aulasDoDia(const QDate &data)
{
    QList<AulaDoDia> resultado;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT h.id, h.turma_id, h.dia_semana, h.hora_inicio, h.hora_fim, h.sala, "
        "t.nome, t.disciplina, t.cor, "
        "COALESCE((SELECT a.tema FROM aulas a WHERE a.turma_id = h.turma_id AND a.data = :data "
        "          ORDER BY a.id LIMIT 1), '') "
        "FROM horarios h JOIN turmas t ON t.id = h.turma_id "
        "WHERE t.arquivada = 0 AND h.dia_semana = :dia "
        "ORDER BY h.hora_inicio"));
    ligar(q, QStringLiteral(":data"), data.toString(Qt::ISODate));
    ligar(q, QStringLiteral(":dia"), data.dayOfWeek());  // 1 = segunda ... 7 = domingo

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next()) {
        AulaDoDia a;
        a.horario.id = q.value(0).toInt();
        a.horario.turmaId = q.value(1).toInt();
        a.horario.diaSemana = q.value(2).toInt();
        a.horario.inicio = QTime::fromString(q.value(3).toString(), QStringLiteral("HH:mm"));
        a.horario.fim = QTime::fromString(q.value(4).toString(), QStringLiteral("HH:mm"));
        a.horario.sala = q.value(5).toString();
        a.horario.turmaNome = q.value(6).toString();
        a.horario.turmaDisciplina = q.value(7).toString();
        a.horario.turmaCor = q.value(8).toString();
        a.tema = q.value(9).toString();
        resultado.append(a);
    }
    return resultado;
}

QList<AgendaRepository::AvaliacaoDatada> AgendaRepository::avaliacoesDatadas(const QDate &de, const QDate &ate)
{
    QList<AvaliacaoDatada> resultado;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT a.data, a.nome, a.tipo, t.nome, t.cor FROM avaliacoes a JOIN turmas t ON t.id = a.turma_id "
        "WHERE t.arquivada = 0 AND a.data IS NOT NULL AND a.data BETWEEN :de AND :ate ORDER BY a.data, a.id"));
    ligar(q, QStringLiteral(":de"), de.toString(Qt::ISODate));
    ligar(q, QStringLiteral(":ate"), ate.toString(Qt::ISODate));

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next()) {
        AvaliacaoDatada a;
        a.data = QDate::fromString(q.value(0).toString(), Qt::ISODate);
        a.nome = q.value(1).toString();
        a.tipo = q.value(2).toString();
        a.turmaNome = q.value(3).toString();
        a.turmaCor = q.value(4).toString();
        if (a.data.isValid())
            resultado.append(a);
    }
    return resultado;
}

QList<AgendaRepository::ProvaProxima> AgendaRepository::provasProximas(const QDate &desde, int dias)
{
    QList<ProvaProxima> resultado;
    QSqlQuery q;
    // substr(...,1,10) pega só a parte da data, caso o evento guarde data e hora.
    q.prepare(QStringLiteral(
        "SELECT substr(e.data_inicio, 1, 10) AS dia, e.titulo, COALESCE(t.nome, ''), COALESCE(t.cor, '#4C8BF5') "
        "FROM eventos e LEFT JOIN turmas t ON t.id = e.turma_id "
        "WHERE e.tipo = 'prova' AND substr(e.data_inicio, 1, 10) BETWEEN :de AND :ate "
        "UNION ALL "
        "SELECT a.data, a.nome, t.nome, t.cor "
        "FROM avaliacoes a JOIN turmas t ON t.id = a.turma_id "
        "WHERE a.tipo = 'prova' AND t.arquivada = 0 AND a.data BETWEEN :de AND :ate "
        "ORDER BY 1, 3"));
    ligar(q, QStringLiteral(":de"), desde.toString(Qt::ISODate));
    ligar(q, QStringLiteral(":ate"), desde.addDays(dias).toString(Qt::ISODate));

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next()) {
        ProvaProxima p;
        p.data = QDate::fromString(q.value(0).toString(), Qt::ISODate);
        p.titulo = q.value(1).toString();
        p.turmaNome = q.value(2).toString();
        p.turmaCor = q.value(3).toString();
        if (p.data.isValid())
            resultado.append(p);
    }
    return resultado;
}
