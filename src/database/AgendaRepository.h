#pragma once

#include "models/Horario.h"

#include <QDate>
#include <QList>
#include <QString>

// Consultas só de leitura que alimentam o painel "Hoje".
class AgendaRepository {
public:
    struct AulaDoDia {
        Horario horario;
        QString tema;  // tema do plano de aula daquele dia (vazio se não houver plano)
    };

    struct ProvaProxima {
        QDate data;
        QString titulo;
        QString turmaNome;
        QString turmaCor;
    };

    // Avaliação (prova, trabalho...) com data marcada, para o calendário.
    struct AvaliacaoDatada {
        QDate data;
        QString nome;
        QString tipo;
        QString turmaNome;
        QString turmaCor;
    };

    // Avaliações das turmas ativas com data entre `de` e `ate`.
    QList<AvaliacaoDatada> avaliacoesDatadas(const QDate &de, const QDate &ate);

    // Aulas da grade semanal que caem no dia da semana de `data`, em ordem de horário,
    // com o tema do plano de aula (tabela "aulas") quando existir para essa data.
    QList<AulaDoDia> aulasDoDia(const QDate &data);

    // Provas entre `desde` e `desde + dias`: eventos do calendário do tipo "prova"
    // e avaliações do tipo "prova" que têm data marcada.
    QList<ProvaProxima> provasProximas(const QDate &desde, int dias);

    QString ultimoErro() const { return m_erro; }

private:
    QString m_erro;
};
