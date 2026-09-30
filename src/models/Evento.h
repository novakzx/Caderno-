#pragma once

#include <QDate>
#include <QString>

// Modelo de dados: evento do calendário escolar (prova, feriado, reunião...).
struct Evento {
    int id = 0;
    int turmaId = 0;        // 0 = evento geral (vale para todas as turmas)
    QString titulo;
    QString tipo = QStringLiteral("evento");  // prova, feriado, recesso, reuniao, evento
    QDate dataInicio;
    QDate dataFim;          // inválida = evento de um dia só
    QString descricao;

    QString turmaNome;      // preenchidos pelo repositório para exibição
    QString turmaCor;

    QDate ultimoDia() const { return dataFim.isValid() ? dataFim : dataInicio; }
};
