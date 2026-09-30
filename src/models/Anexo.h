#pragma once

#include <QString>

// Modelo de dados: um arquivo (apresentação, PDF, planilha...) vinculado a uma
// turma e, opcionalmente, a uma aula. Só o CAMINHO é guardado: o arquivo em si
// continua onde está e abre no programa padrão do sistema.
struct Anexo {
    int id = 0;
    int turmaId = 0;   // 0 = sem turma
    int aulaId = 0;    // 0 = anexo da turma (não de uma aula específica)
    int alunoId = 0;   // 0 = sem aluno
    QString nome;      // nome exibido (por padrão, o nome do arquivo)
    QString caminho;   // caminho completo no disco
    QString tipo;      // extensão em minúsculas (pptx, pdf...)
    QString criadoEm;

    QString aulaTema;  // preenchido pelo repositório (tema da aula, se houver)
};
