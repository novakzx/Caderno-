// Teste simples (sem framework) das regras dos lembretes: quando avisar de aulas, tarefas e provas.
// Rodar: ctest --test-dir build   (ou executar o binário test_lembrete)
#include "core/LembreteUtil.h"

#include <cstdio>

static int falhas = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FALHOU (linha %d): %s\n", __LINE__, #cond);       \
            ++falhas;                                                      \
        }                                                                  \
    } while (0)

using namespace LembreteUtil;

int main()
{
    const int inicio = 7 * 60 + 30;  // aula às 07:30

    // --- Aula: só dentro da janela de antecedência, nunca depois de começar ---
    CHECK(minutosParaAvisarAula(inicio - 10, inicio, 10) == 10);   // 07:20, janela de 10 min: avisa (faltam 10)
    CHECK(minutosParaAvisarAula(inicio - 5, inicio, 10) == 5);
    CHECK(minutosParaAvisarAula(inicio - 1, inicio, 10) == 1);
    CHECK(minutosParaAvisarAula(inicio - 11, inicio, 10) == -1);   // cedo demais
    CHECK(minutosParaAvisarAula(inicio, inicio, 10) == -1);        // já começou
    CHECK(minutosParaAvisarAula(inicio + 5, inicio, 10) == -1);    // passou
    CHECK(minutosParaAvisarAula(inicio - 5, inicio, 0) == -1);     // 0 = desligado
    CHECK(minutosParaAvisarAula(inicio - 30, inicio, 30) == 30);
    CHECK(minutosParaAvisarAula(inicio - 5, inicio, -3) == -1);    // valor absurdo = desligado

    // --- Prazos: hoje e amanhã, a partir das 8h ---
    CHECK(deveAvisarPrazo(0, 8));
    CHECK(deveAvisarPrazo(1, 8));
    CHECK(deveAvisarPrazo(1, 17));
    CHECK(!deveAvisarPrazo(0, 7));    // de madrugada, não
    CHECK(!deveAvisarPrazo(2, 12));   // ainda longe
    CHECK(!deveAvisarPrazo(-1, 12));  // atrasada: o painel Hoje já mostra
    CHECK(!deveAvisarPrazo(-30, 12));

    // --- Antecedências oferecidas ---
    CHECK(antecedenciaValida(0));
    CHECK(antecedenciaValida(10));
    CHECK(antecedenciaValida(30));
    CHECK(!antecedenciaValida(7));
    CHECK(!antecedenciaValida(-5));

    // --- Padrões ---
    const Config padrao;
    CHECK(padrao.ativos && padrao.prazos && padrao.antecedenciaAula == 10);

    if (falhas == 0)
        std::printf("OK: regras dos lembretes.\n");
    return falhas == 0 ? 0 : 1;
}
