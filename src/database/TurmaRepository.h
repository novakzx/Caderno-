#pragma once

#include "models/Turma.h"

#include <QList>
#include <QString>
#include <optional>

// Repositório de turmas: todo o SQL sobre a tabela "turmas" fica aqui.
// Os widgets só conversam com esta classe, nunca com o banco diretamente.
class TurmaRepository {
public:
    QList<Turma> listar(bool incluirArquivadas = false);
    std::optional<Turma> buscar(int id);

    // Insere e devolve o novo id (ou 0 em caso de erro; veja ultimoErro()).
    int inserir(const Turma &turma);
    bool atualizar(const Turma &turma);
    // Apaga a turma e, por cascata, alunos/notas/frequência dela.
    bool remover(int id);

    QString ultimoErro() const { return m_erro; }

private:
    QString m_erro;
};
