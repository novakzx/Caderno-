#pragma once

#include "models/Avaliacao.h"

#include <QList>
#include <QString>
#include <optional>

// Repositório de avaliações (colunas da planilha de notas).
class AvaliacaoRepository {
public:
    // Avaliações da turma ordenadas por período, ordem e id.
    // periodo = 0 lista todos os períodos.
    QList<Avaliacao> listarPorTurma(int turmaId, int periodo = 0);
    std::optional<Avaliacao> buscar(int id);

    // Insere no fim do período (ordem = última + 1). Devolve o id ou 0 se falhar.
    int inserir(const Avaliacao &avaliacao);
    bool atualizar(const Avaliacao &avaliacao);
    // Apaga a avaliação e, por cascata, as notas lançadas nela.
    bool remover(int id);

    // Troca a posição da avaliação com a vizinha do mesmo período
    // (delta = -1 move para a esquerda, +1 para a direita).
    bool mover(int id, int delta);

    QString ultimoErro() const { return m_erro; }

private:
    QString m_erro;
};
