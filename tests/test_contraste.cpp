// Teste simples (sem framework) dos tokens de cor do design system.
// Garante texto >= 4.5:1 e controles >= 3:1 nos dois temas, nos pares de cor que o
// programa realmente usa. Se um token do design mudar e quebrar o contraste, falha aqui.
// Rodar: ctest --test-dir build   (ou executar o binário test_contraste)
#include "core/Contraste.h"
#include "core/Tokens.h"

#include <cstdio>
#include <cstring>

static int falhas = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FALHOU (linha %d): %s\n", __LINE__, #cond);       \
            ++falhas;                                                      \
        }                                                                  \
    } while (0)

using Tokens::Id;

// Verifica o par (texto, fundo) nos dois temas contra o mínimo `minimo`.
static void par(Id texto, Id fundo, double minimo)
{
    for (const bool escuro : {false, true}) {
        const double r = Contraste::razao(Tokens::hex(texto, escuro), Tokens::hex(fundo, escuro));
        if (r < minimo) {
            std::printf("FALHOU: %s sobre %s no tema %s: %.2f:1 (minimo %.1f:1)\n",
                        Tokens::definicao(texto).nome, Tokens::definicao(fundo).nome,
                        escuro ? "escuro" : "claro", r, minimo);
            ++falhas;
        }
    }
}

int main()
{
    // Os nomes da tabela batem com o enum (ordem) e com o nome de turma.
    CHECK(std::strcmp(Tokens::definicao(Id::Surface100).nome, "surface-100") == 0);
    CHECK(std::strcmp(Tokens::definicao(Id::Focus).nome, "focus") == 0);
    CHECK(std::strcmp(Tokens::nomeDaTurma(1), "turma-1") == 0);
    CHECK(std::strcmp(Tokens::nomeDaTurma(6), "turma-6") == 0);
    CHECK(std::strcmp(Tokens::nomeDaTurma(0), "turma-6") == 0);   // fora da faixa -> padrão
    CHECK(Tokens::numeroDaTurma("turma-4") == 4);
    CHECK(Tokens::numeroDaTurma("turma-7") == 0);
    CHECK(Tokens::numeroDaTurma("#4C8BF5") == 0);                 // cor antiga em hex
    CHECK(Tokens::numeroDaTurma(nullptr) == 0);

    // turma-6 e focus são o primary (referências {primary} do tokens.json).
    for (const bool escuro : {false, true}) {
        CHECK(std::strcmp(Tokens::hex(Id::Turma6, escuro), Tokens::hex(Id::Primary, escuro)) == 0);
        CHECK(std::strcmp(Tokens::hex(Id::Focus, escuro), Tokens::hex(Id::Primary, escuro)) == 0);
    }

    // Referência conhecida da fórmula: preto sobre branco = 21:1.
    CHECK(Contraste::razao("#000000", "#ffffff") > 20.99);

    // --- Texto (>= 4.5:1) ---
    for (Id fundo : {Id::Surface100, Id::Surface200, Id::Surface300, Id::PrimarySoft, Id::AccentSoft})
        par(Id::Ink, fundo, 4.5);
    for (Id fundo : {Id::Surface100, Id::Surface200, Id::Surface300, Id::PrimarySoft})
        par(Id::InkMuted, fundo, 4.5);
    for (Id fundo : {Id::Surface100, Id::Surface200, Id::Surface300, Id::PrimarySoft})
        par(Id::Primary, fundo, 4.5);            // links, item ativo da barra lateral
    par(Id::OnPrimary, Id::Primary, 4.5);        // botão principal
    for (int n = 1; n <= Tokens::kTotalTurmas; ++n)
        par(Id::OnTurma, Tokens::turma(n), 4.5); // texto dentro do bloco da turma
    for (Id estado : {Id::Success, Id::Warning, Id::Danger})
        for (Id fundo : {Id::Surface100, Id::Surface200})
            par(estado, fundo, 4.5);             // P / F / J, avisos e erros

    // --- Controles e marcadores (>= 3:1) ---
    par(Id::LineStrong, Id::Surface200, 3.0);    // borda de campos
    par(Id::Focus, Id::Surface100, 3.0);         // anel de foco
    par(Id::Focus, Id::Surface200, 3.0);

    if (falhas == 0)
        std::printf("OK: tokens de cor e contraste.\n");
    return falhas == 0 ? 0 : 1;
}
