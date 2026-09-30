#pragma once

#include <optional>
#include <vector>

// Cálculo da média ponderada. Só usa a biblioteca padrão (sem Qt), para poder
// ser testado isoladamente (veja tests/test_media.cpp).
namespace MediaCalculator {

struct Item {
    double valor;       // nota lançada
    double peso;        // peso da avaliação
    double notaMaxima;  // nota máxima da avaliação
};

// Média ponderada na escala `escala` (padrão 0-10).
//
// Regras:
//  - cada nota é primeiro normalizada para a escala (valor / notaMaxima * escala),
//    assim uma prova valendo 5,0 pontos e outra valendo 10,0 podem ser misturadas;
//  - só entram avaliações que já têm nota lançada (o chamador passa só elas):
//    a média é "parcial" até todas as notas estarem preenchidas;
//  - itens com peso <= 0 ou notaMaxima <= 0 são ignorados;
//  - sem nenhum item válido, não há média (std::nullopt).
inline std::optional<double> ponderada(const std::vector<Item> &itens, double escala = 10.0)
{
    double soma = 0.0;
    double pesos = 0.0;
    for (const Item &i : itens) {
        if (i.peso <= 0.0 || i.notaMaxima <= 0.0)
            continue;
        soma += (i.valor / i.notaMaxima * escala) * i.peso;
        pesos += i.peso;
    }
    if (pesos <= 0.0)
        return std::nullopt;
    return soma / pesos;
}

}  // namespace MediaCalculator
