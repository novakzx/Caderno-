#pragma once

#include <array>
#include <cctype>
#include <optional>
#include <string>

// Números de versão "1.2.3" (com ou sem "v" na frente), sem Qt (testável: tests/test_utilitarios.cpp).
// Usado para saber se a Release mais recente do GitHub é mais nova que o programa em execução.
namespace VersaoUtil {

using Versao = std::array<int, 3>;

// "v1.2.3" ou "1.2.3" -> {1,2,3}. Qualquer outra coisa (inclusive "1.2", "1.2.3-beta", "1.2.3.4") é recusada:
// só números inteiros pequenos, nada de texto solto vindo de fora.
inline std::optional<Versao> analisar(const std::string &texto)
{
    std::size_t i = (!texto.empty() && (texto[0] == 'v' || texto[0] == 'V')) ? 1 : 0;
    Versao v{0, 0, 0};
    for (int parte = 0; parte < 3; ++parte) {
        if (i >= texto.size() || !std::isdigit(static_cast<unsigned char>(texto[i])))
            return std::nullopt;
        long valor = 0;
        int digitos = 0;
        while (i < texto.size() && std::isdigit(static_cast<unsigned char>(texto[i]))) {
            valor = valor * 10 + (texto[i] - '0');
            ++i;
            if (++digitos > 6)
                return std::nullopt;
        }
        v[parte] = static_cast<int>(valor);
        if (parte < 2) {
            if (i >= texto.size() || texto[i] != '.')
                return std::nullopt;
            ++i;
        }
    }
    if (i != texto.size())
        return std::nullopt;
    return v;
}

// A versão `remota` é estritamente mais nova que a `atual`? Versão ilegível nunca é "mais nova".
inline bool ehMaisNova(const std::string &remota, const std::string &atual)
{
    const auto r = analisar(remota);
    const auto a = analisar(atual);
    if (!r || !a)
        return false;
    return *r > *a;
}

}  // namespace VersaoUtil
