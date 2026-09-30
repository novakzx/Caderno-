#pragma once

#include <cstddef>
#include <cstring>

// Tokens de cor do design system do Caderno+ (espelho de design/tokens.json).
//
// Este é o ÚNICO lugar do código com valores hexadecimais de cor. O restante do
// programa pede a cor pelo NOME do token (veja ThemeManager::cor). Lógica pura, sem Qt,
// para poder ser testada em tests/test_contraste.cpp.
//
// Ao mudar o tokens.json, atualize esta tabela e rode os testes.
namespace Tokens {

enum class Id {
    Surface100,  // fundo da página
    Surface200,  // cartões, painéis, campos, barra lateral
    Surface300,  // preenchimento sutil (hover, trilho de abas)
    Line,        // divisórias e bordas de cartão
    LineStrong,  // borda de controles (3:1)
    Ink,         // texto principal
    InkMuted,    // texto secundário
    Primary,     // verde-lousa: ação principal, item ativo, foco
    OnPrimary,   // texto sobre primary
    PrimarySoft, // fundo de seleção
    Accent,      // ocre: marcadores não textuais
    AccentSoft,  // fundo de destaque
    Success,
    Warning,
    Danger,
    Turma1,
    Turma2,
    Turma3,
    Turma4,
    Turma5,
    Turma6,      // padrão de turma nova
    OnTurma,     // texto sobre qualquer turma-N
    Focus,
    Total
};

struct Definicao {
    const char *nome;    // nome no tokens.json
    const char *claro;   // "#rrggbb"
    const char *escuro;  // "#rrggbb"
};

// Mesma ordem do enum Id.
inline constexpr Definicao kTabela[] = {
    {"surface-100", "#f7f5f0", "#161a19"},
    {"surface-200", "#ffffff", "#1f2422"},
    {"surface-300", "#eeebe4", "#29302d"},
    {"line",        "#dcd7cd", "#36403c"},
    {"line-strong", "#8a918b", "#6f7a74"},
    {"ink",         "#1f2623", "#eef0ec"},
    {"ink-muted",   "#5c655f", "#a7b0aa"},
    {"primary",     "#2f6b5a", "#6fbfa5"},
    {"on-primary",  "#ffffff", "#0f1f1a"},
    {"primary-soft", "#e3efe9", "#24453b"},
    {"accent",      "#c98a2b", "#e0a64b"},
    {"accent-soft", "#f6ead3", "#3a2e1a"},
    {"success",     "#2f7d4a", "#6cc58d"},
    {"warning",     "#8f5b00", "#e8b04d"},
    {"danger",      "#b3261e", "#f08a80"},
    {"turma-1",     "#36729a", "#7fb3d5"},
    {"turma-2",     "#94641a", "#e0a64b"},
    {"turma-3",     "#7f5492", "#c39ad6"},
    {"turma-4",     "#a84f35", "#e59077"},
    {"turma-5",     "#557034", "#a3c47a"},
    {"turma-6",     "#2f6b5a", "#6fbfa5"},  // = {primary}
    {"on-turma",    "#ffffff", "#121614"},
    {"focus",       "#2f6b5a", "#6fbfa5"},  // = {primary}
};

inline constexpr std::size_t kTotal = static_cast<std::size_t>(Id::Total);
static_assert(sizeof(kTabela) / sizeof(kTabela[0]) == kTotal, "tabela de tokens fora de sincronia com o enum");

inline constexpr int kTotalTurmas = 6;

inline const Definicao &definicao(Id id)
{
    return kTabela[static_cast<std::size_t>(id)];
}

inline const char *hex(Id id, bool escuro)
{
    return escuro ? definicao(id).escuro : definicao(id).claro;
}

// Token de turma pelo número 1..6 (fora disso, devolve a turma-6, o padrão).
inline Id turma(int numero)
{
    if (numero < 1 || numero > kTotalTurmas)
        return Id::Turma6;
    return static_cast<Id>(static_cast<int>(Id::Turma1) + numero - 1);
}

// Valor gravado no banco para a cor de uma turma: o NOME do token ("turma-3").
// Assim a cor acompanha o tema claro/escuro.
inline const char *nomeDaTurma(int numero)
{
    return definicao(turma(numero)).nome;
}

// "turma-3" -> 3; qualquer outra coisa (inclusive "#rrggbb" antigo) -> 0.
inline int numeroDaTurma(const char *nome)
{
    if (nome == nullptr || std::strlen(nome) != 7 || std::strncmp(nome, "turma-", 6) != 0)
        return 0;
    const int n = nome[6] - '0';
    return (n >= 1 && n <= kTotalTurmas) ? n : 0;
}

}  // namespace Tokens
