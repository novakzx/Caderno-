#pragma once

#include <algorithm>
#include <cctype>
#include <iterator>
#include <string>

// Regras das contas locais (login do programa): validação de nome, e-mail e senha, força
// da senha e tempo de bloqueio após tentativas erradas. Lógica pura, sem Qt, testada em
// tests/test_conta.cpp. Os textos são UTF-8.
//
// Atenção: a conta protege o ACESSO ao programa e separa os dados de cada pessoa que usa o
// computador, mas o arquivo do banco não é criptografado (quem tiver acesso ao arquivo
// consegue ler). Veja docs/SEGURANCA.md.
namespace ContaUtil {

constexpr int kSenhaMinima = 8;
constexpr int kSenhaMaxima = 128;
constexpr int kTentativasAntesDoBloqueio = 5;
constexpr long long kBloqueioMaximoSegundos = 15 * 60;

struct Resultado {
    bool ok = true;
    std::string motivo;  // mensagem para o usuário quando !ok
};

inline std::string aparar(const std::string &s)
{
    const auto espaco = [](unsigned char c) { return std::isspace(c) != 0; };
    auto ini = std::find_if_not(s.begin(), s.end(), espaco);
    auto fim = std::find_if_not(s.rbegin(), s.rend(), espaco).base();
    return ini < fim ? std::string(ini, fim) : std::string();
}

inline std::string minusculas(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Conta os caracteres de um texto UTF-8 (não os bytes).
inline int comprimentoUtf8(const std::string &s)
{
    int n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80)
            ++n;
    return n;
}

inline Resultado validarNome(const std::string &nome)
{
    const int n = comprimentoUtf8(aparar(nome));
    if (n < 2)
        return {false, "Informe seu nome (pelo menos 2 letras)."};
    if (n > 80)
        return {false, "O nome é longo demais (máximo de 80 caracteres)."};
    return {};
}

// Verificação simples e segura: uma "@", algo antes e um domínio com ponto depois, sem espaços.
inline Resultado validarEmail(const std::string &email)
{
    const std::string e = aparar(email);
    const auto arroba = e.find('@');
    const bool formaOk = arroba != std::string::npos && arroba > 0 && e.find('@', arroba + 1) == std::string::npos;
    bool dominioOk = false;
    if (formaOk) {
        const std::string dominio = e.substr(arroba + 1);
        const auto ponto = dominio.rfind('.');
        dominioOk = ponto != std::string::npos && ponto > 0 && ponto + 1 < dominio.size() &&
                    dominio.front() != '.' && dominio.find("..") == std::string::npos;
    }
    const bool semEspaco = std::none_of(e.begin(), e.end(), [](unsigned char c) { return std::isspace(c) != 0; });
    if (!formaOk || !dominioOk || !semEspaco || e.size() > 254)
        return {false, "Informe um e-mail válido (por exemplo, nome@escola.com)."};
    return {};
}

// Senhas comuns demais para serem aceitas (comparadas em minúsculas).
inline bool senhaComum(const std::string &minuscula)
{
    static const char *comuns[] = {"12345678", "123456789", "1234567890", "password", "password1", "senha123",
                                   "senha1234", "qwertyui", "qwerty123", "abcd1234", "11111111", "00000000",
                                   "professor", "professor1", "professora", "caderno1", "iloveyou", "admin123"};
    return std::any_of(std::begin(comuns), std::end(comuns), [&](const char *c) { return minuscula == c; });
}

// Regras: 8 a 128 caracteres, com letras E números (ou 12+ caracteres), fora da lista de
// senhas comuns e sem conter o e-mail/nome da pessoa.
inline Resultado validarSenha(const std::string &senha, const std::string &nome = {}, const std::string &email = {})
{
    const int n = comprimentoUtf8(senha);
    if (n < kSenhaMinima)
        return {false, "A senha precisa ter pelo menos 8 caracteres."};
    if (n > kSenhaMaxima)
        return {false, "A senha é longa demais (máximo de 128 caracteres)."};

    const bool temLetra = std::any_of(senha.begin(), senha.end(), [](unsigned char c) { return std::isalpha(c) || c >= 0x80; });
    const bool temNumero = std::any_of(senha.begin(), senha.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
    if ((!temLetra || !temNumero) && n < 12)
        return {false, "Use letras e números na senha (ou uma frase com 12 ou mais caracteres)."};

    const std::string minuscula = minusculas(senha);
    if (senhaComum(minuscula))
        return {false, "Essa senha é muito comum. Escolha outra."};

    const std::string antesDoArroba = minusculas(aparar(email)).substr(0, minusculas(aparar(email)).find('@'));
    if (antesDoArroba.size() >= 4 && minuscula.find(antesDoArroba) != std::string::npos)
        return {false, "A senha não deve conter o seu e-mail."};
    const std::string primeiroNome = minusculas(aparar(nome)).substr(0, minusculas(aparar(nome)).find(' '));
    if (primeiroNome.size() >= 4 && minuscula.find(primeiroNome) != std::string::npos)
        return {false, "A senha não deve conter o seu nome."};
    return {};
}

// Força da senha de 0 (fraca) a 4 (forte), só para a barrinha de indicação na tela.
inline int forcaDaSenha(const std::string &senha)
{
    const int n = comprimentoUtf8(senha);
    if (n == 0)
        return 0;
    int pontos = 0;
    if (n >= 8) ++pontos;
    if (n >= 12) ++pontos;
    const auto tem = [&](auto pred) { return std::any_of(senha.begin(), senha.end(), pred); };
    const bool minusc = tem([](unsigned char c) { return std::islower(c) != 0; });
    const bool maiusc = tem([](unsigned char c) { return std::isupper(c) != 0; });
    const bool digito = tem([](unsigned char c) { return std::isdigit(c) != 0; });
    const bool simbolo = tem([](unsigned char c) { return !std::isalnum(c) && c < 0x80; });
    if ((minusc && maiusc) || (minusc && digito) || simbolo) ++pontos;
    if (minusc + maiusc + digito + simbolo >= 3) ++pontos;
    if (senhaComum(minusculas(senha)))
        pontos = std::min(pontos, 1);
    return std::min(pontos, 4);
}

// Bloqueio depois de errar a senha várias vezes seguidas: 30 s na 5ª falha e dobrando a
// cada nova falha, até 15 minutos. Atrapalha quem tenta adivinhar a senha sem atrapalhar
// quem só errou uma ou duas vezes.
inline long long segundosDeBloqueio(int falhasSeguidas)
{
    if (falhasSeguidas < kTentativasAntesDoBloqueio)
        return 0;
    long long s = 30;
    for (int i = kTentativasAntesDoBloqueio; i < falhasSeguidas && s < kBloqueioMaximoSegundos; ++i)
        s *= 2;
    return std::min(s, kBloqueioMaximoSegundos);
}

// Comparação que leva o mesmo tempo qualquer que seja a diferença (evita "vazar" a
// posição do primeiro byte diferente pelo tempo de resposta).
inline bool iguaisEmTempoConstante(const std::string &a, const std::string &b)
{
    unsigned char diferenca = a.size() == b.size() ? 0 : 1;
    const std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i)
        diferenca |= static_cast<unsigned char>(a[i] ^ b[i]);
    return diferenca == 0;
}

}  // namespace ContaUtil
