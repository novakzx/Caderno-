#pragma once

#include <QDate>
#include <QString>

// Modelo de dados: um aluno, sempre vinculado a uma turma.
struct Aluno {
    int id = 0;
    int turmaId = 0;
    QString nome;
    QString matricula;        // única dentro da turma (quando preenchida)
    QString email;
    QDate dataNascimento;     // data inválida (QDate()) = não informada
    QString observacoes;
    bool ativo = true;        // false = transferido/desistente (mantém histórico)
};
