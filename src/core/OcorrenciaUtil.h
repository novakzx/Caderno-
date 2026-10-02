#pragma once

#include <string_view>

// Tipos de ocorrência registrada sobre um aluno, sem Qt (testável: tests/test_atencao.cpp).
// O banco guarda o `id` (texto); `rotulo` é o que a pessoa lê na tela e nos relatórios.
//
// "Negativa" = ocorrência que conta como sinal de alerta no painel "Alunos em atenção".
// Para criar um tipo novo basta acrescentar uma linha aqui (nenhuma migração é necessária).
namespace OcorrenciaUtil {

struct Tipo {
    const char *id;
    const char *rotulo;
    bool negativa;
};

inline constexpr Tipo kTipos[] = {
    {"elogio", "Elogio", false},
    {"conduta", "Conduta", true},
    {"dificuldade", "Dificuldade de aprendizagem", true},
    {"familia", "Contato com a família", false},
    {"outro", "Outro", false},
};
inline constexpr int kQuantidadeDeTipos = static_cast<int>(sizeof(kTipos) / sizeof(kTipos[0]));

inline bool idValido(std::string_view id)
{
    for (const Tipo &t : kTipos)
        if (id == t.id)
            return true;
    return false;
}

// Tipo pelo id; id desconhecido (ex.: dado de uma versão futura) cai em "outro".
inline const Tipo &tipoDe(std::string_view id)
{
    for (const Tipo &t : kTipos)
        if (id == t.id)
            return t;
    return kTipos[kQuantidadeDeTipos - 1];
}

inline bool ehNegativa(std::string_view id)
{
    return idValido(id) && tipoDe(id).negativa;
}

}  // namespace OcorrenciaUtil
