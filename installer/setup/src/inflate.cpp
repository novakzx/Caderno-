#include "inflate.h"

#include <cstring>

namespace Inflate {

namespace {

constexpr int kBitsMaximos = 15;
constexpr int kLitCompr = 286;
constexpr int kDistCompr = 30;
constexpr int kCodigosFixos = 288;

struct Estado {
    const std::uint8_t *entrada;
    std::size_t tamEntrada;
    std::size_t posEntrada = 0;
    std::uint32_t acumulador = 0;
    int bitsNoAcumulador = 0;

    std::uint8_t *saida;
    std::size_t tamSaida;
    std::size_t posSaida = 0;
    bool erro = false;
};

struct Huffman {
    std::uint16_t contagem[kBitsMaximos + 1];
    std::uint16_t simbolo[kCodigosFixos];
};

// Lê `n` bits (n <= 16), do menos para o mais significativo.
int bits(Estado &s, int n)
{
    std::uint32_t valor = s.acumulador;
    while (s.bitsNoAcumulador < n) {
        if (s.posEntrada >= s.tamEntrada) {
            s.erro = true;
            return 0;
        }
        valor |= static_cast<std::uint32_t>(s.entrada[s.posEntrada++]) << s.bitsNoAcumulador;
        s.bitsNoAcumulador += 8;
    }
    s.acumulador = valor >> n;
    s.bitsNoAcumulador -= n;
    return static_cast<int>(valor & ((1u << n) - 1));
}

// Constrói a tabela de decodificação a partir dos comprimentos dos códigos.
// Devolve 0 se o código for completo, > 0 se incompleto, < 0 se super-assinado.
int construir(Huffman &h, const std::uint16_t *comprimento, int n)
{
    for (int len = 0; len <= kBitsMaximos; ++len)
        h.contagem[len] = 0;
    for (int sim = 0; sim < n; ++sim)
        ++h.contagem[comprimento[sim]];
    if (h.contagem[0] == n)
        return 0;

    int sobra = 1;
    for (int len = 1; len <= kBitsMaximos; ++len) {
        sobra <<= 1;
        sobra -= h.contagem[len];
        if (sobra < 0)
            return sobra;
    }
    std::uint16_t deslocamento[kBitsMaximos + 1];
    deslocamento[1] = 0;
    for (int len = 1; len < kBitsMaximos; ++len)
        deslocamento[len + 1] = static_cast<std::uint16_t>(deslocamento[len] + h.contagem[len]);
    for (int sim = 0; sim < n; ++sim)
        if (comprimento[sim] != 0)
            h.simbolo[deslocamento[comprimento[sim]]++] = static_cast<std::uint16_t>(sim);
    return sobra;
}

int decodificar(Estado &s, const Huffman &h)
{
    int codigo = 0, primeiro = 0, indice = 0;
    for (int len = 1; len <= kBitsMaximos; ++len) {
        codigo |= bits(s, 1);
        if (s.erro)
            return -1;
        const int contagem = h.contagem[len];
        if (codigo - contagem < primeiro)
            return h.simbolo[indice + (codigo - primeiro)];
        indice += contagem;
        primeiro += contagem;
        primeiro <<= 1;
        codigo <<= 1;
    }
    return -1;  // código inválido
}

bool codigos(Estado &s, const Huffman &lit, const Huffman &dist)
{
    static const std::uint16_t compr[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const std::uint16_t extraCompr[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const std::uint16_t dists[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const std::uint16_t extraDist[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

    for (;;) {
        int simbolo = decodificar(s, lit);
        if (simbolo < 0)
            return false;
        if (simbolo < 256) {
            if (s.posSaida >= s.tamSaida)
                return false;
            s.saida[s.posSaida++] = static_cast<std::uint8_t>(simbolo);
        } else if (simbolo == 256) {
            return true;  // fim do bloco
        } else {
            simbolo -= 257;
            if (simbolo >= 29)
                return false;
            const int tamanho = compr[simbolo] + bits(s, extraCompr[simbolo]);
            const int d = decodificar(s, dist);
            if (d < 0 || d >= 30)
                return false;
            const std::size_t distancia = static_cast<std::size_t>(dists[d] + bits(s, extraDist[d]));
            if (s.erro || distancia > s.posSaida || s.posSaida + static_cast<std::size_t>(tamanho) > s.tamSaida)
                return false;
            for (int i = 0; i < tamanho; ++i, ++s.posSaida)
                s.saida[s.posSaida] = s.saida[s.posSaida - distancia];
        }
    }
}

bool blocoSemCompressao(Estado &s)
{
    s.acumulador = 0;  // descarta os bits restantes do byte atual
    s.bitsNoAcumulador = 0;
    if (s.posEntrada + 4 > s.tamEntrada)
        return false;
    unsigned len = s.entrada[s.posEntrada] | (s.entrada[s.posEntrada + 1] << 8);
    const unsigned nlen = s.entrada[s.posEntrada + 2] | (s.entrada[s.posEntrada + 3] << 8);
    s.posEntrada += 4;
    if (len != (~nlen & 0xFFFFu))
        return false;
    if (s.posEntrada + len > s.tamEntrada || s.posSaida + len > s.tamSaida)
        return false;
    std::memcpy(s.saida + s.posSaida, s.entrada + s.posEntrada, len);
    s.posEntrada += len;
    s.posSaida += len;
    return true;
}

bool blocoFixo(Estado &s)
{
    static bool pronto = false;
    static Huffman lit, dist;
    if (!pronto) {
        std::uint16_t comprimento[kCodigosFixos];
        int sim = 0;
        for (; sim < 144; ++sim) comprimento[sim] = 8;
        for (; sim < 256; ++sim) comprimento[sim] = 9;
        for (; sim < 280; ++sim) comprimento[sim] = 7;
        for (; sim < kCodigosFixos; ++sim) comprimento[sim] = 8;
        construir(lit, comprimento, kCodigosFixos);
        for (sim = 0; sim < kDistCompr; ++sim) comprimento[sim] = 5;
        construir(dist, comprimento, kDistCompr);
        pronto = true;
    }
    return codigos(s, lit, dist);
}

bool blocoDinamico(Estado &s)
{
    static const std::uint8_t ordem[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    const int nlen = bits(s, 5) + 257;
    const int ndist = bits(s, 5) + 1;
    const int ncode = bits(s, 4) + 4;
    if (s.erro || nlen > kLitCompr + 2 || ndist > kDistCompr + 2)
        return false;

    std::uint16_t comprimento[kLitCompr + 2 + kDistCompr + 2];
    int indice = 0;
    for (; indice < ncode; ++indice)
        comprimento[ordem[indice]] = static_cast<std::uint16_t>(bits(s, 3));
    for (; indice < 19; ++indice)
        comprimento[ordem[indice]] = 0;
    if (s.erro)
        return false;

    Huffman cod;
    if (construir(cod, comprimento, 19) != 0)
        return false;

    indice = 0;
    while (indice < nlen + ndist) {
        int simbolo = decodificar(s, cod);
        if (simbolo < 0)
            return false;
        if (simbolo < 16) {
            comprimento[indice++] = static_cast<std::uint16_t>(simbolo);
        } else {
            int anterior = 0, repetir;
            if (simbolo == 16) {
                if (indice == 0)
                    return false;
                anterior = comprimento[indice - 1];
                repetir = 3 + bits(s, 2);
            } else if (simbolo == 17) {
                repetir = 3 + bits(s, 3);
            } else {
                repetir = 11 + bits(s, 7);
            }
            if (s.erro || indice + repetir > nlen + ndist)
                return false;
            while (repetir--)
                comprimento[indice++] = static_cast<std::uint16_t>(anterior);
        }
    }
    if (comprimento[256] == 0)
        return false;  // sem código de fim de bloco

    Huffman lit, dist;
    int r = construir(lit, comprimento, nlen);
    if (r < 0 || (r > 0 && nlen - lit.contagem[0] != 1))
        return false;
    r = construir(dist, comprimento + nlen, ndist);
    if (r < 0 || (r > 0 && ndist - dist.contagem[0] != 1))
        return false;
    return codigos(s, lit, dist);
}

}  // namespace

bool descomprimir(const std::uint8_t *entrada, std::size_t tamanhoEntrada, std::uint8_t *saida, std::size_t tamanhoSaida)
{
    Estado s;
    s.entrada = entrada;
    s.tamEntrada = tamanhoEntrada;
    s.saida = saida;
    s.tamSaida = tamanhoSaida;

    int ultimo;
    do {
        ultimo = bits(s, 1);
        const int tipo = bits(s, 2);
        if (s.erro)
            return false;
        bool ok;
        switch (tipo) {
        case 0: ok = blocoSemCompressao(s); break;
        case 1: ok = blocoFixo(s); break;
        case 2: ok = blocoDinamico(s); break;
        default: return false;
        }
        if (!ok)
            return false;
    } while (!ultimo);
    return s.posSaida == s.tamSaida;
}

std::uint32_t crc32(const std::uint8_t *dados, std::size_t tamanho, std::uint32_t inicial)
{
    static std::uint32_t tabela[256];
    static bool pronta = false;
    if (!pronta) {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            tabela[i] = c;
        }
        pronta = true;
    }
    std::uint32_t c = ~inicial;
    for (std::size_t i = 0; i < tamanho; ++i)
        c = tabela[(c ^ dados[i]) & 0xFF] ^ (c >> 8);
    return ~c;
}

}  // namespace Inflate
