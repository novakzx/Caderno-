#include "database/HorarioRepository.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {

const char *FORMATO_HORA = "HH:mm";  // horas gravadas como texto "07:30" (ordena e compara corretamente)

const char *SELECT_BASE =
    "SELECT h.id, h.turma_id, h.dia_semana, h.hora_inicio, h.hora_fim, h.sala, "
    "t.nome, t.disciplina, t.cor "
    "FROM horarios h JOIN turmas t ON t.id = h.turma_id ";

Horario lerHorario(const QSqlQuery &q)
{
    Horario h;
    h.id = q.value(0).toInt();
    h.turmaId = q.value(1).toInt();
    h.diaSemana = q.value(2).toInt();
    h.inicio = QTime::fromString(q.value(3).toString(), QLatin1String(FORMATO_HORA));
    h.fim = QTime::fromString(q.value(4).toString(), QLatin1String(FORMATO_HORA));
    h.sala = q.value(5).toString();
    h.turmaNome = q.value(6).toString();
    h.turmaDisciplina = q.value(7).toString();
    h.turmaCor = q.value(8).toString();
    return h;
}

}  // namespace

QList<Horario> HorarioRepository::listar()
{
    QList<Horario> resultado;
    QSqlQuery q;
    if (!q.exec(QStringLiteral("%1WHERE t.arquivada = 0 ORDER BY h.dia_semana, h.hora_inicio")
                    .arg(QLatin1String(SELECT_BASE)))) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next())
        resultado.append(lerHorario(q));
    return resultado;
}

std::optional<Horario> HorarioRepository::buscar(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("%1WHERE h.id = :id").arg(QLatin1String(SELECT_BASE)));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return std::nullopt;
    }
    if (!q.next())
        return std::nullopt;
    return lerHorario(q);
}

int HorarioRepository::inserir(const Horario &h)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO horarios (turma_id, dia_semana, hora_inicio, hora_fim, sala) "
        "VALUES (:turma, :dia, :inicio, :fim, :sala)"));
    q.bindValue(QStringLiteral(":turma"), h.turmaId);
    q.bindValue(QStringLiteral(":dia"), h.diaSemana);
    q.bindValue(QStringLiteral(":inicio"), h.inicio.toString(QLatin1String(FORMATO_HORA)));
    q.bindValue(QStringLiteral(":fim"), h.fim.toString(QLatin1String(FORMATO_HORA)));
    q.bindValue(QStringLiteral(":sala"), h.sala);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return 0;
    }
    return q.lastInsertId().toInt();
}

bool HorarioRepository::atualizar(const Horario &h)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "UPDATE horarios SET turma_id = :turma, dia_semana = :dia, hora_inicio = :inicio, "
        "hora_fim = :fim, sala = :sala WHERE id = :id"));
    q.bindValue(QStringLiteral(":turma"), h.turmaId);
    q.bindValue(QStringLiteral(":dia"), h.diaSemana);
    q.bindValue(QStringLiteral(":inicio"), h.inicio.toString(QLatin1String(FORMATO_HORA)));
    q.bindValue(QStringLiteral(":fim"), h.fim.toString(QLatin1String(FORMATO_HORA)));
    q.bindValue(QStringLiteral(":sala"), h.sala);
    q.bindValue(QStringLiteral(":id"), h.id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

bool HorarioRepository::remover(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM horarios WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

bool HorarioRepository::temConflito(int diaSemana, const QTime &inicio, const QTime &fim, int ignorarId)
{
    // Dois intervalos se sobrepõem quando cada um começa antes de o outro terminar.
    // Blocos encostados (07:30-08:20 e 08:20-09:10) NÃO conflitam.
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM horarios h JOIN turmas t ON t.id = h.turma_id "
        "WHERE t.arquivada = 0 AND h.dia_semana = :dia AND h.id <> :ignorar "
        "AND h.hora_inicio < :fim AND :inicio < h.hora_fim"));
    q.bindValue(QStringLiteral(":dia"), diaSemana);
    q.bindValue(QStringLiteral(":ignorar"), ignorarId);
    q.bindValue(QStringLiteral(":inicio"), inicio.toString(QLatin1String(FORMATO_HORA)));
    q.bindValue(QStringLiteral(":fim"), fim.toString(QLatin1String(FORMATO_HORA)));

    if (!q.exec() || !q.next()) {
        m_erro = q.lastError().text();
        return false;
    }
    return q.value(0).toInt() > 0;
}
