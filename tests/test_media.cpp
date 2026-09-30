// Teste simples (sem framework) do cálculo de média ponderada.
// Rodar: ctest --test-dir build   (ou executar o binário test_media)
#include "core/MediaCalculator.h"

#include <cmath>
#include <cstdio>

static int falhas = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FALHOU (linha %d): %s\n", __LINE__, #cond);       \
            ++falhas;                                                      \
        }                                                                  \
    } while (0)

static bool quase(double a, double b) { return std::fabs(a - b) < 1e-9; }

int main()
{
    using namespace MediaCalculator;

    // Sem notas -> sem média.
    CHECK(!ponderada({}).has_value());

    // Média simples (pesos iguais): (8 + 6) / 2 = 7
    CHECK(quase(*ponderada({{8, 1, 10}, {6, 1, 10}}), 7.0));

    // Média ponderada: (9*2 + 6*1) / 3 = 8
    CHECK(quase(*ponderada({{9, 2, 10}, {6, 1, 10}}), 8.0));

    // Notas em escalas diferentes são normalizadas: 4/5 = 8,0 e 6/10 = 6,0 -> 7,0
    CHECK(quase(*ponderada({{4, 1, 5}, {6, 1, 10}}), 7.0));

    // Itens inválidos (peso 0 ou máximo 0) são ignorados.
    CHECK(quase(*ponderada({{10, 0, 10}, {5, 1, 10}, {9, 1, 0}}), 5.0));
    CHECK(!ponderada({{10, 0, 10}}).has_value());

    if (falhas == 0)
        std::printf("Todos os testes passaram.\n");
    return falhas == 0 ? 0 : 1;
}
