#pragma once

#include "services/XlsxService.h"

#include <QList>
#include <QString>
#include <QStringList>

class AlunoRepository;
class AvaliacaoRepository;
class NotaRepository;

// Plano de importação: o que o programa pretende fazer com a planilha, antes
// de gravar qualquer coisa. A tela de importação mostra este plano e deixa o
// professor ajustá-lo (desmarcar colunas, trocar o destino).
struct PlanoImportacao {
    struct Coluna {
        QString nome;
        int avaliacaoExistenteId = 0;  // 0 = criar nova avaliação
        double peso = 1.0;             // usados só ao criar
        double notaMaxima = 10.0;
        int periodo = 1;
        bool importar = true;
    };

    QList<Coluna> colunas;            // alinhadas com Planilha::colunas
    QList<int> alunoIdPorLinha;       // alinhado com Planilha::linhas; 0 = aluno não encontrado
    QStringList alunosNaoEncontrados; // nomes (ou matrículas) sem correspondência
    int alunosEncontrados = 0;
};

struct ResultadoImportacao {
    int avaliacoesCriadas = 0;
    int notasGravadas = 0;
    int notasIgnoradas = 0;  // fora do intervalo 0..nota máxima
};

// Regras de importação (sem nenhuma interface gráfica):
//  - alunos NÃO são criados: só recebem nota os que já existem na turma
//    (casados por matrícula; se não houver, pelo nome sem acentos/maiúsculas);
//  - colunas com o mesmo nome de uma avaliação existente atualizam essa
//    avaliação; as demais criam avaliações novas;
//  - célula vazia na planilha NÃO apaga a nota existente;
//  - tudo é gravado numa única transação: se algo falhar, nada é alterado.
class ImportadorNotas {
public:
    ImportadorNotas(AvaliacaoRepository &avaliacoes, AlunoRepository &alunos, NotaRepository &notas)
        : m_avaliacoes(avaliacoes), m_alunos(alunos), m_notas(notas) {}

    PlanoImportacao planejar(int turmaId, const XlsxService::Planilha &planilha);

    bool executar(int turmaId, const XlsxService::Planilha &planilha, const PlanoImportacao &plano,
                  ResultadoImportacao *resultado, QString *erro);

private:
    AvaliacaoRepository &m_avaliacoes;
    AlunoRepository &m_alunos;
    NotaRepository &m_notas;
};
