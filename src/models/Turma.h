#pragma once

#include <QString>

// Modelo de dados: uma turma (ex.: "8º A - Matemática").
// É um struct simples: não conhece banco nem interface.
struct Turma {
    int id = 0;               // 0 = ainda não gravada no banco
    QString nome;
    QString disciplina;
    int anoLetivo = 0;
    QString periodo;          // ex.: "Manhã", "1º semestre"
    QString sala;
    QString cor = QStringLiteral("turma-6");  // token "turma-1".."turma-6" (ou "#rrggbb" antigo); veja ThemeManager::corDaTurma
    bool arquivada = false;

    // Campo calculado pelo repositório (não existe na tabela): alunos ativos.
    int totalAlunos = 0;
};
