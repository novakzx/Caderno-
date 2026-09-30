#pragma once

#include <QDate>
#include <QString>

// Modelo de dados: plano de uma aula de uma turma em uma data.
struct Aula {
    int id = 0;
    int turmaId = 0;
    QDate data;
    QString tema;
    QString objetivos;
    QString materiais;
    QString observacoes;

    QString turmaNome;  // preenchido pelo repositório para exibição
};
