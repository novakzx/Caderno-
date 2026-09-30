// Teste simples (sem framework) do cálculo de frequência.
#include "core/FrequenciaUtil.h"

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
    using namespace FrequenciaUtil;

    // Sem chamadas -> sem percentual
    CHECK(!percentual(0, 0, 0, 0).has_value());

    // 8 presenças + 2 faltas = 80%
    CHECK(quase(*percentual(8, 0, 2, 0), 80.0));

    // Atraso conta como presença: 7 P + 1 A + 2 F = 80%
    CHECK(quase(*percentual(7, 1, 2, 0), 80.0));

    // Falta justificada não reduz: 8 P + 2 J = 100%
    CHECK(quase(*percentual(8, 0, 0, 2), 100.0));

    // Só faltas = 0%
    CHECK(quase(*percentual(0, 0, 4, 0), 0.0));

    // Limite de 75%: 3 P + 1 F = exatamente 75% (não abaixo do mínimo)
    CHECK(*percentual(3, 0, 1, 0) >= kFrequenciaMinima);
    CHECK(*percentual(2, 0, 1, 0) < kFrequenciaMinima);

    if (falhas == 0)
        std::printf("Todos os testes passaram.\n");
    return falhas == 0 ? 0 : 1;
}
