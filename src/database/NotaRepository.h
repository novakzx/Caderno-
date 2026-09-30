#pragma once

#include <QHash>
#include <QString>
#include <optional>

// Repositório de notas: cada nota é uma célula (avaliação x aluno).
class NotaRepository {
public:
    // Chave para indexar notas em memória: (avaliacaoId, alunoId) -> qint64.
    static qint64 chave(int avaliacaoId, int alunoId)
    {
        return (static_cast<qint64>(avaliacaoId) << 32) | static_cast<quint32>(alunoId);
    }

    // Todas as notas lançadas (não nulas) das avaliações da turma.
    QHash<qint64, double> listarPorTurma(int turmaId);

    // Grava a nota (insere ou atualiza). valor == nullopt apaga a nota.
    // Não abre transação própria: para gravar em lote, o chamador abre a transação.
    bool salvar(int avaliacaoId, int alunoId, std::optional<double> valor);

    // Quantas notas da avaliação são maiores que `limite` (usado ao reduzir a nota máxima).
    int contarAcima(int avaliacaoId, double limite);

    QString ultimoErro() const { return m_erro; }

private:
    QString m_erro;
};
