// Teste simples (sem framework) das regras de conta: nome, e-mail, senha, bloqueio.
// Rodar: ctest --test-dir build   (ou executar o binário test_conta)
#include "core/ContaUtil.h"

#include <cstdio>

static int falhas = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FALHOU (linha %d): %s\n", __LINE__, #cond);       \
            ++falhas;                                                      \
        }                                                                  \
    } while (0)

using namespace ContaUtil;

int main()
{
    // --- Nome ---
    CHECK(validarNome("Ana").ok);
    CHECK(validarNome("  Jo  ").ok);
    CHECK(!validarNome("").ok);
    CHECK(!validarNome("   ").ok);
    CHECK(!validarNome("A").ok);
    CHECK(validarNome("Antônio José").ok);        // acentos contam como 1 caractere cada
    CHECK(!validarNome(std::string(81, 'a')).ok);

    // --- E-mail ---
    CHECK(validarEmail("ana@escola.com").ok);
    CHECK(validarEmail("  ana.souza+aulas@escola.edu.br ").ok);
    CHECK(!validarEmail("").ok);
    CHECK(!validarEmail("ana").ok);
    CHECK(!validarEmail("ana@").ok);
    CHECK(!validarEmail("@escola.com").ok);
    CHECK(!validarEmail("ana@escola").ok);
    CHECK(!validarEmail("ana@@escola.com").ok);
    CHECK(!validarEmail("ana @escola.com").ok);
    CHECK(!validarEmail("ana@escola..com").ok);
    CHECK(!validarEmail("ana@.com").ok);
    CHECK(!validarEmail("ana@escola.").ok);
    CHECK(!validarEmail(std::string(250, 'a') + "@b.com").ok);

    // --- Senha ---
    CHECK(validarSenha("Giz2026!", "Ana", "ana@escola.com").ok);
    CHECK(validarSenha("lousa-verde-azul-sol").ok);          // frase longa sem número
    CHECK(!validarSenha("curta1").ok);                       // curta
    CHECK(validarSenha("somenteletras").ok);                 // 13 caracteres: aceita como frase
    CHECK(!validarSenha("abcdefgh").ok);                     // 8 letras sem número
    CHECK(!validarSenha("12345678").ok);                     // só números
    CHECK(!validarSenha("senha123").ok);                     // comum
    CHECK(!validarSenha("Password1").ok);                    // comum (ignora maiúsculas)
    CHECK(!validarSenha(std::string(129, 'a') + "1").ok);    // longa demais
    CHECK(!validarSenha("ana.souza2026x", "Ana Souza", "ana.souza@escola.com").ok);  // contém o e-mail
    CHECK(!validarSenha("Mariana2026", "Mariana Lima", "x@y.com").ok);               // contém o nome
    CHECK(validarSenha("Giz2026!", "Marcos", "m@y.com").ok);

    // --- Força (0..4) ---
    CHECK(forcaDaSenha("") == 0);
    CHECK(forcaDaSenha("abc") < 2);
    CHECK(forcaDaSenha("Giz2026!") >= 3);
    CHECK(forcaDaSenha("lousa-verde-azul-sol-2026") == 4);
    CHECK(forcaDaSenha("password") <= 1);

    // --- Bloqueio ---
    CHECK(segundosDeBloqueio(0) == 0);
    CHECK(segundosDeBloqueio(4) == 0);
    CHECK(segundosDeBloqueio(5) == 30);
    CHECK(segundosDeBloqueio(6) == 60);
    CHECK(segundosDeBloqueio(7) == 120);
    CHECK(segundosDeBloqueio(100) == kBloqueioMaximoSegundos);

    // --- Comparação em tempo constante ---
    CHECK(iguaisEmTempoConstante("abc", "abc"));
    CHECK(!iguaisEmTempoConstante("abc", "abd"));
    CHECK(!iguaisEmTempoConstante("abc", "abcd"));
    CHECK(iguaisEmTempoConstante("", ""));

    if (falhas == 0)
        std::printf("OK: regras de conta.\n");
    return falhas == 0 ? 0 : 1;
}
