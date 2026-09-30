#pragma once

#include "models/Aluno.h"

#include <QList>
#include <QString>
#include <optional>

// Repositório de alunos: todo o SQL sobre a tabela "alunos" fica aqui.
class AlunoRepository {
public:
    // Alunos de uma turma, em ordem alfabética. `filtro` procura em nome,
    // matrícula e e-mail (vazio = sem filtro).
    QList<Aluno> listarPorTurma(int turmaId, const QString &filtro = QString(),
                                bool incluirInativos = true);
    std::optional<Aluno> buscar(int id);

    int inserir(const Aluno &aluno);  // devolve o novo id, ou 0 em caso de erro
    bool atualizar(const Aluno &aluno);
    bool remover(int id);

    QString ultimoErro() const { return m_erro; }

private:
    QString m_erro;
};
