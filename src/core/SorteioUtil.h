#pragma once

#include <algorithm>
#include <cstddef>
#include <random>
#include <vector>

// Sorteios para a sala de aula, sem Qt (testável: tests/test_utilitarios.cpp).
// O gerador de números aleatórios vem de fora: o programa usa um sorteado de verdade e os testes usam semente fixa.
namespace SorteioUtil {

// Uma ordem aleatória dos índices 0..n-1.
inline std::vector<int> embaralhar(int n, std::mt19937 &rng)
{
    std::vector<int> indices(static_cast<std::size_t>(n < 0 ? 0 : n));
    for (std::size_t i = 0; i < indices.size(); ++i)
        indices[i] = static_cast<int>(i);
    std::shuffle(indices.begin(), indices.end(), rng);
    return indices;
}

// Divide `n` pessoas em `quantidadeDeGrupos` grupos de tamanhos o mais iguais possível
// (a diferença entre o maior e o menor grupo é no máximo 1). Cada grupo é uma lista de índices.
// Quantidade inválida (< 1) vira 1 grupo; mais grupos que pessoas vira um grupo por pessoa.
inline std::vector<std::vector<int>> dividirEmGrupos(int n, int quantidadeDeGrupos, std::mt19937 &rng)
{
    std::vector<std::vector<int>> grupos;
    if (n <= 0)
        return grupos;
    const int total = std::max(1, std::min(quantidadeDeGrupos, n));
    grupos.resize(static_cast<std::size_t>(total));
    const std::vector<int> ordem = embaralhar(n, rng);
    for (std::size_t i = 0; i < ordem.size(); ++i)
        grupos[i % static_cast<std::size_t>(total)].push_back(ordem[i]);  // distribuição "em rodízio"
    return grupos;
}

// Quantos grupos são necessários para que cada um tenha, no máximo, `tamanho` pessoas.
inline int gruposParaTamanho(int n, int tamanho)
{
    if (n <= 0)
        return 0;
    if (tamanho < 1)
        tamanho = 1;
    return (n + tamanho - 1) / tamanho;
}

// Sorteia sem repetir: devolve a próxima pessoa de uma fila embaralhada. Quando todas já saíram,
// a fila é refeita (e `acabou` fica verdadeiro na primeira chamada depois de esgotar a anterior).
class SorteioSemRepeticao {
public:
    explicit SorteioSemRepeticao(int n) : m_n(n < 0 ? 0 : n) {}

    // -1 se não há ninguém para sortear.
    int proximo(std::mt19937 &rng, bool *recomecou = nullptr)
    {
        if (recomecou)
            *recomecou = false;
        if (m_n == 0)
            return -1;
        if (m_fila.empty()) {
            if (m_jaSorteou && recomecou)
                *recomecou = true;
            m_fila = embaralhar(m_n, rng);
            m_jaSorteou = true;
        }
        const int escolhido = m_fila.back();
        m_fila.pop_back();
        return escolhido;
    }
    // Quantos ainda não saíram nesta rodada (antes do primeiro sorteio, todos).
    int restantes() const { return m_jaSorteou ? static_cast<int>(m_fila.size()) : m_n; }

private:
    int m_n = 0;
    bool m_jaSorteou = false;
    std::vector<int> m_fila;
};

}  // namespace SorteioUtil
