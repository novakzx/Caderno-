#pragma once

#include <QString>
#include <QStringList>

// Modelo de dados: uma anotação em texto rico, com tags e vínculos opcionais
// a turma, aluno e aula.
struct Anotacao {
    int id = 0;
    QString titulo;
    QString conteudoHtml;    // texto formatado (o que o editor mostra)
    QString conteudoTexto;   // mesmo texto sem formatação (usado na busca)
    int turmaId = 0;         // 0 = sem vínculo
    int alunoId = 0;
    int aulaId = 0;
    QString criadaEm;
    QString atualizadaEm;
    QStringList tags;
};

// Versão enxuta para a lista lateral (evita carregar o HTML inteiro de cada nota).
struct AnotacaoResumo {
    int id = 0;
    QString titulo;
    QString atualizadaEm;
    QString turmaNome;
    QString alunoNome;
    QString trecho;       // começo do texto, sem formatação
    QStringList tags;
};
