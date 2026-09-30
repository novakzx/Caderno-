#pragma once

#include "models/Aluno.h"
#include "models/Avaliacao.h"
#include "models/Frequencia.h"
#include "models/Turma.h"

#include <QList>
#include <QVector>
#include <optional>

struct Repositorios;

// Uma linha do boletim: um aluno, suas notas, média e frequência.
struct LinhaBoletim {
    int alunoId = 0;
    QString nome;
    QString matricula;
    QVector<std::optional<double>> notas;  // alinhadas com Boletim::avaliacoes
    std::optional<double> media;           // média ponderada (0-10)
    ResumoFrequencia frequencia;
    std::optional<double> frequenciaPct;   // nullopt = sem chamadas registradas
};

// Dados consolidados de uma turma (alimenta os gráficos e os relatórios em PDF).
struct Boletim {
    Turma turma;
    int periodo = 0;                        // 0 = todos os períodos
    QList<Avaliacao> avaliacoes;
    QList<LinhaBoletim> linhas;
    std::optional<double> mediaTurma;
    QVector<std::optional<double>> mediaPorAvaliacao;  // média da turma em cada avaliação (escala 0-10)
};

// Dados de um aluno para a ficha individual.
struct FichaAluno {
    Aluno aluno;
    Turma turma;
    QList<Avaliacao> avaliacoes;            // todos os períodos
    QVector<std::optional<double>> notas;   // alinhadas com avaliacoes
    std::optional<double> media;
    ResumoFrequencia frequencia;
    std::optional<double> frequenciaPct;
    QList<RegistroFrequencia> ocorrencias;  // faltas, justificadas e atrasos (mais recentes primeiro)
};

// Junta notas, médias e frequência. Só orquestra repositórios e regras puras
// (MediaCalculator, FrequenciaUtil): não conhece widgets.
class DesempenhoService {
public:
    explicit DesempenhoService(Repositorios &repos) : m_repos(repos) {}

    std::optional<Boletim> boletim(int turmaId, int periodo);
    std::optional<FichaAluno> ficha(int alunoId);

    // Quantos alunos caem em cada faixa de média (faixas iguais entre 0 e `maximo`).
    static QVector<int> distribuicaoDeMedias(const Boletim &boletim, int faixas = 5, double maximo = 10.0);

private:
    Repositorios &m_repos;
};
