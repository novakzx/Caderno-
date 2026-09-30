#pragma once

#include "models/Evento.h"

#include <QDate>
#include <QList>
#include <QString>
#include <optional>

// Repositório do calendário escolar (tabela "eventos").
class EventoRepository {
public:
    // Eventos que tocam o intervalo [de, ate] (inclusive eventos de vários dias).
    QList<Evento> listarPeriodo(const QDate &de, const QDate &ate);
    QList<Evento> listarDoDia(const QDate &dia) { return listarPeriodo(dia, dia); }
    std::optional<Evento> buscar(int id);

    int inserir(const Evento &evento);  // devolve o novo id, ou 0 em caso de erro
    bool atualizar(const Evento &evento);
    bool remover(int id);

    // Título do feriado/recesso que cobre o dia (vazio se for dia letivo normal).
    QString motivoDeDiaSemAula(const QDate &dia);

    QString ultimoErro() const { return m_erro; }

private:
    QString m_erro;
};
