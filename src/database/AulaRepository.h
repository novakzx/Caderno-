#pragma once

#include "models/Aula.h"

#include <QList>
#include <QString>
#include <optional>

// Repositório de planos de aula (tabela "aulas").
class AulaRepository {
public:
    // Da mais recente para a mais antiga. turmaId = 0 lista de todas as turmas
    // (não arquivadas).
    QList<Aula> listar(int turmaId = 0);
    std::optional<Aula> buscar(int id);

    int inserir(const Aula &aula);  // devolve o novo id, ou 0 em caso de erro
    bool atualizar(const Aula &aula);
    bool remover(int id);           // anexos da aula são apagados em cascata

    QString ultimoErro() const { return m_erro; }

private:
    QString m_erro;
};
