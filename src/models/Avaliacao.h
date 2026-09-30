#pragma once

#include <QDate>
#include <QString>

// Modelo de dados: uma avaliação = uma coluna da planilha de notas
// (prova, trabalho, atividade...), com peso e nota máxima próprios.
struct Avaliacao {
    int id = 0;
    int turmaId = 0;
    QString nome;
    QString tipo = QStringLiteral("prova");  // prova, trabalho, atividade, participacao, outro
    double peso = 1.0;                       // usado na média ponderada
    double notaMaxima = 10.0;                // valor máximo aceito na coluna
    QDate data;                              // opcional (data inválida = não informada)
    int periodo = 1;                         // bimestre/trimestre (1 a 4)
    int ordem = 0;                           // posição da coluna dentro do período
};
