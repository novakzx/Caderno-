#pragma once

#include "models/Anotacao.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

// Repositório de anotações e tags (tabelas "anotacoes", "tags" e "anotacao_tags").
class AnotacaoRepository {
public:
    struct Filtro {
        QString texto;     // procura no título e no texto
        QString tag;       // vazio = qualquer tag
        int turmaId = 0;   // 0 = qualquer turma
        int alunoId = 0;
    };

    // Da mais recentemente alterada para a mais antiga.
    // (Sem argumento padrão "Filtro = {}": um struct aninhado com valores padrão
    //  não pode ser usado como argumento padrão dentro da própria classe.)
    QList<AnotacaoResumo> listar(const Filtro &filtro, int limite = 500);
    QList<AnotacaoResumo> listar() { return listar(Filtro(), 500); }
    std::optional<Anotacao> buscar(int id);

    // Gravam a anotação e as tags na mesma transação.
    int inserir(const Anotacao &anotacao);  // devolve o novo id, ou 0 em caso de erro
    bool atualizar(const Anotacao &anotacao);
    bool remover(int id);

    QStringList listarTags();  // todas as tags em uso, em ordem alfabética

    QString ultimoErro() const { return m_erro; }

private:
    bool definirTags(int anotacaoId, const QStringList &tags);

    QString m_erro;
};
