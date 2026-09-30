#pragma once

#include <cmath>
#include <cstdlib>
#include <utility>

// Contraste de cores (WCAG 2.x), em lógica pura. Usado nos testes para garantir que
// os pares de tokens do design system mantêm texto >= 4.5:1 nos dois temas.
namespace Contraste {

// "#rrggbb" -> luminância relativa (0..1).
inline double luminancia(const char *hex)
{
    if (hex == nullptr || hex[0] != '#')
        return 0.0;
    auto canal = [&](int i) {
        const char par[3] = {hex[1 + i * 2], hex[2 + i * 2], '\0'};
        const double c = std::strtol(par, nullptr, 16) / 255.0;
        return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * canal(0) + 0.7152 * canal(1) + 0.0722 * canal(2);
}

// Razão de contraste entre duas cores "#rrggbb" (1.0 a 21.0).
inline double razao(const char *a, const char *b)
{
    double la = luminancia(a);
    double lb = luminancia(b);
    if (la < lb)
        std::swap(la, lb);
    return (la + 0.05) / (lb + 0.05);
}

}  // namespace Contraste
