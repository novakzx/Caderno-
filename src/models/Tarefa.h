#pragma once

#include <QDate>
#include <QString>

// Modelo de dados: uma tarefa do professor (corrigir provas, preparar aula...).
struct Tarefa {
    int id = 0;
    int turmaId = 0;          // 0 = tarefa sem turma
    QString titulo;
    QString descricao;
    QDate dataEntrega;        // data inválida = sem prazo
    bool concluida = false;
    int prioridade = 1;       // 0 baixa, 1 normal, 2 alta

    QString turmaNome;        // preenchido pelo repositório para exibição
};
