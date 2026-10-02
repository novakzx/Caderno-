#pragma once

#include <cstddef>
#include <cstdint>

// Descompressão "deflate" (RFC 1951) e CRC-32, sem dependências: o instalador é um único .exe.
// O pacote é comprimido por installer/empacotar.py (zlib, nível 9, deflate "cru").
namespace Inflate {

// Descomprime exatamente `tamanhoSaida` bytes. Devolve false se os dados estiverem corrompidos
// (ou se a saída não couber/ não for do tamanho esperado). Nunca escreve fora de `saida`.
bool descomprimir(const std::uint8_t *entrada, std::size_t tamanhoEntrada, std::uint8_t *saida, std::size_t tamanhoSaida);

std::uint32_t crc32(const std::uint8_t *dados, std::size_t tamanho, std::uint32_t inicial = 0);

}  // namespace Inflate
