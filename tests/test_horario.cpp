// Teste simples (sem framework) das contas de horário da grade semanal.
#include "core/HorarioUtil.h"

#include <cstdio>

static int falhas = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FALHOU (linha %d): %s\n", __LINE__, #cond);       \
            ++falhas;                                                      \
        }                                                                  \
    } while (0)

int main()
{
    using namespace HorarioUtil;

    // Arredondamento de 5 em 5 minutos
    CHECK(arredondar(60, 5) == 60);
    CHECK(arredondar(62, 5) == 60);
    CHECK(arredondar(63, 5) == 65);
    CHECK(arredondar(-10, 5) == 0);

    // Sobreposição: blocos encostados não conflitam
    CHECK(!sobrepoe(450, 500, 500, 550));
    CHECK(!sobrepoe(500, 550, 450, 500));
    CHECK(sobrepoe(450, 500, 480, 530));
    CHECK(sobrepoe(450, 600, 480, 500));  // um contém o outro

    // Ajuste dentro dos limites (07:00 = 420 .. 18:00 = 1080), bloco de 50 min
    CHECK(ajustarInicio(1100, 50, 420, 1080) == 1030);  // não pode passar do fim
    CHECK(ajustarInicio(300, 50, 420, 1080) == 420);    // não pode passar do começo
    CHECK(ajustarInicio(452, 50, 420, 1080) == 450);    // arredonda
    // Bloco maior que a janela: fica no começo em vez de quebrar
    CHECK(ajustarInicio(500, 900, 420, 1080) == 420);

    if (falhas == 0)
        std::printf("Todos os testes passaram.\n");
    return falhas == 0 ? 0 : 1;
}
