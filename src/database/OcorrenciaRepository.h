#pragma once

#include "models/Ocorrencia.h"

#include <QDate>
#include <QHash>
#include <QList>
#include <QString>
#include <optional>

// Repositório das ocorrências por aluno: todo o SQL sobre a tabela "ocorrencias" fica aqui.
class OcorrenciaRepository {
public:
    // Do aluno, da mais recente para a mais antiga.
    QList<Ocorrencia> listarPorAluno(int alunoId);
    std::optional<Ocorrencia> buscar(int id);

    int inserir(const Ocorrencia &ocorrencia);  // devolve o novo id, ou 0 em caso de erro
    bool atualizar(const Ocorrencia &ocorrencia);
    bool remover(int id);

    // Quantas ocorrências NEGATIVAS (OcorrenciaUtil) cada aluno da turma tem desde `desde`.
    // Só aparecem no resultado os alunos com pelo menos uma.
    QHash<int, int> contarNegativasPorAluno(int turmaId, const QDate &desde);

    // Total de ocorrências de cada aluno da turma (alunos sem nenhuma não aparecem).
    QHash<int, int> contarPorAluno(int turmaId);

    QString ultimoErro() const { return m_erro; }

private:
    QString m_erro;
};
