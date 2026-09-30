#pragma once

#include <QString>
#include <QTime>

// Modelo de dados: um bloco de aula na grade semanal (ex.: 8º A, segunda, 07:30-08:20).
struct Horario {
    int id = 0;
    int turmaId = 0;
    int diaSemana = 1;     // 1 = segunda ... 7 = domingo (igual a Qt::DayOfWeek)
    QTime inicio;
    QTime fim;
    QString sala;

    // Campos da turma, preenchidos pelo repositório para exibição (não gravados aqui).
    QString turmaNome;
    QString turmaDisciplina;
    QString turmaCor = QStringLiteral("turma-6");  // token da turma (ThemeManager::corDaTurma)
};
