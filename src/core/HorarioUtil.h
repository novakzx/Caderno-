#pragma once

#include <algorithm>

// Contas de horário em minutos desde 00:00, sem Qt (testável: tests/test_horario.cpp).
// Usadas pela grade semanal para arrastar/redimensionar blocos de aula.
namespace HorarioUtil {

// Arredonda para o múltiplo mais próximo de `passo` (ex.: 62 -> 60, 63 -> 65 com passo 5).
inline int arredondar(int minutos, int passo)
{
    if (minutos < 0)
        minutos = 0;
    return ((minutos + passo / 2) / passo) * passo;
}

// Dois intervalos [ini, fim) se sobrepõem? Blocos encostados (um termina quando
// o outro começa) NÃO contam como sobreposição.
inline bool sobrepoe(int ini1, int fim1, int ini2, int fim2)
{
    return ini1 < fim2 && ini2 < fim1;
}

// Início (arredondado) para um bloco de `duracao` minutos que o usuário soltou
// em `inicioDesejado`, mantendo o bloco inteiro dentro de [minimo, maximo].
inline int ajustarInicio(int inicioDesejado, int duracao, int minimo, int maximo, int passo = 5)
{
    const int ini = arredondar(inicioDesejado, passo);
    return std::clamp(ini, minimo, std::max(minimo, maximo - duracao));
}

}  // namespace HorarioUtil
