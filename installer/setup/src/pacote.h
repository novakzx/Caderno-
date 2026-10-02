#pragma once

#include <windows.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// O pacote do instalador: os arquivos do programa, comprimidos, ANEXADOS ao final do próprio .exe
// (montado por installer/empacotar.py). Formato (todos os números em little-endian):
//
//   [ instalador (.exe) ]            <- "stub": tamanhoDoStub bytes
//   [ entrada ] x quantidade         <- u16 tamanhoDoCaminho | caminho UTF-8 com "/" | u64 tamanho | u64 comprimido
//                                       | u32 crc32 | u8 método (0 = guardado, 1 = deflate) | dados
//   [ metadados ]                    <- texto UTF-8, uma linha "chave=valor" cada
//   [ rodapé: 48 bytes ]             <- "CDRNPAY1" | u64 tamanhoDoStub | u64 deslocamentoMeta | u32 tamanhoMeta
//                                       | u32 quantidade | u64 totalDescomprimido | u32 crcMeta | u32 crcDoRodape
//
// O desinstalador é só o "stub" (sem o pacote): o instalador grava os primeiros `tamanhoDoStub` bytes.
namespace Pacote {

struct Entrada {
    std::string caminho;  // relativo, com "/"
    std::uint64_t tamanho = 0;
    std::uint64_t tamanhoComprimido = 0;
    std::uint32_t crc = 0;
    std::uint8_t metodo = 1;
    std::uint64_t deslocamento = 0;  // onde começam os dados, dentro do arquivo
};

struct Conteudo {
    std::wstring arquivo;
    std::uint64_t tamanhoDoStub = 0;
    std::uint64_t totalDescomprimido = 0;
    std::map<std::string, std::string> meta;
    std::vector<Entrada> entradas;

    std::string valor(const std::string &chave, const std::string &padrao = std::string()) const
    {
        const auto it = meta.find(chave);
        return it == meta.end() ? padrao : it->second;
    }
};

// Lê o rodapé e o índice do pacote que está no fim de `arquivo`. Falso se não houver pacote ou se estiver corrompido.
bool abrir(const std::wstring &arquivo, Conteudo &pacote, std::string &erro);

// Lê, descomprime e confere (CRC-32) uma entrada.
bool lerEntrada(HANDLE arquivo, const Entrada &entrada, std::vector<std::uint8_t> &dados, std::string &erro);

// Copia os primeiros `bytes` de `origem` para `destino` (o desinstalador).
bool copiarInicio(const std::wstring &origem, const std::wstring &destino, std::uint64_t bytes, std::string &erro);

}  // namespace Pacote
