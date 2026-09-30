#pragma once

#include <QChar>
#include <QDate>
#include <QString>

// Um registro de chamada: situação de um aluno em uma data.
//   'P' presente · 'F' falta · 'J' falta justificada · 'A' atraso (conta como presente)
struct RegistroFrequencia {
    int alunoId = 0;
    QDate data;
    QChar situacao = QLatin1Char('P');
    QString justificativa;
};

// Totais de chamada de um aluno em um período.
struct ResumoFrequencia {
    int presencas = 0;
    int faltas = 0;
    int justificadas = 0;
    int atrasos = 0;

    int total() const { return presencas + faltas + justificadas + atrasos; }
};
