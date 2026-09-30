#pragma once

#include <optional>

// Cálculo do percentual de frequência, sem Qt (testável: tests/test_frequencia.cpp).
namespace FrequenciaUtil {

// Mínimo de frequência para aprovação (LDB: 75% da carga horária).
constexpr double kFrequenciaMinima = 75.0;

// Percentual de frequência a partir dos totais de chamada.
//
// Regras:
//  - só conta as chamadas registradas para o aluno;
//  - falta ("F") reduz a frequência; atraso ("A") conta como presença;
//  - falta justificada ("J") NÃO reduz a frequência (é abonada), mas continua
//    aparecendo nos relatórios como justificada;
//  - sem nenhuma chamada registrada, não há percentual (std::nullopt).
inline std::optional<double> percentual(int presencas, int atrasos, int faltas, int justificadas)
{
    const int total = presencas + atrasos + faltas + justificadas;
    if (total <= 0)
        return std::nullopt;
    return 100.0 * (total - faltas) / total;
}

}  // namespace FrequenciaUtil
