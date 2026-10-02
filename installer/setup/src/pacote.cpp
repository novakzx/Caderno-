#include "pacote.h"

#include "inflate.h"

#include <cstring>

namespace Pacote {

namespace {

constexpr char kMagia[8] = {'C', 'D', 'R', 'N', 'P', 'A', 'Y', '1'};
constexpr std::uint32_t kTamanhoDoRodape = 48;
constexpr std::uint64_t kMaiorArquivo = 512ull << 20;  // um arquivo do pacote não passa de 512 MB (sanidade)
constexpr std::uint32_t kMaximoDeArquivos = 20000;

std::uint64_t le(const std::uint8_t *p, int bytes)
{
    std::uint64_t v = 0;
    for (int i = bytes - 1; i >= 0; --i)
        v = (v << 8) | p[i];
    return v;
}

bool posicionar(HANDLE h, std::uint64_t posicao)
{
    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(posicao);
    return SetFilePointerEx(h, li, nullptr, FILE_BEGIN) != 0;
}

bool lerExato(HANDLE h, void *destino, std::size_t tamanho)
{
    auto *p = static_cast<std::uint8_t *>(destino);
    while (tamanho > 0) {
        const DWORD pedido = static_cast<DWORD>(tamanho > (1u << 24) ? (1u << 24) : tamanho);
        DWORD lido = 0;
        if (!ReadFile(h, p, pedido, &lido, nullptr) || lido == 0)
            return false;
        p += lido;
        tamanho -= lido;
    }
    return true;
}

}  // namespace

bool abrir(const std::wstring &arquivo, Conteudo &pacote, std::string &erro)
{
    pacote = Conteudo();
    pacote.arquivo = arquivo;
    HANDLE h = CreateFileW(arquivo.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        erro = "Não foi possível abrir o instalador.";
        return false;
    }
    struct Fechar {
        HANDLE h;
        ~Fechar() { CloseHandle(h); }
    } fechar{h};

    LARGE_INTEGER total;
    if (!GetFileSizeEx(h, &total) || total.QuadPart < static_cast<LONGLONG>(kTamanhoDoRodape)) {
        erro = "sem pacote";
        return false;
    }
    std::uint8_t rodape[kTamanhoDoRodape];
    if (!posicionar(h, static_cast<std::uint64_t>(total.QuadPart) - kTamanhoDoRodape) || !lerExato(h, rodape, sizeof rodape)) {
        erro = "sem pacote";
        return false;
    }
    if (std::memcmp(rodape, kMagia, sizeof kMagia) != 0) {
        erro = "sem pacote";
        return false;
    }
    if (Inflate::crc32(rodape, kTamanhoDoRodape - 4) != static_cast<std::uint32_t>(le(rodape + 44, 4))) {
        erro = "O instalador está corrompido (rodapé inválido). Baixe-o de novo.";
        return false;
    }
    pacote.tamanhoDoStub = le(rodape + 8, 8);
    const std::uint64_t deslocamentoMeta = le(rodape + 16, 8);
    const std::uint32_t tamanhoMeta = static_cast<std::uint32_t>(le(rodape + 24, 4));
    const std::uint32_t quantidade = static_cast<std::uint32_t>(le(rodape + 28, 4));
    pacote.totalDescomprimido = le(rodape + 32, 8);
    const std::uint32_t crcMeta = static_cast<std::uint32_t>(le(rodape + 40, 4));

    const std::uint64_t tamanhoDoArquivo = static_cast<std::uint64_t>(total.QuadPart);
    if (quantidade > kMaximoDeArquivos || tamanhoMeta > (1u << 20) || pacote.tamanhoDoStub > deslocamentoMeta ||
        deslocamentoMeta + tamanhoMeta + kTamanhoDoRodape != tamanhoDoArquivo) {
        erro = "O instalador está corrompido (índice inválido). Baixe-o de novo.";
        return false;
    }

    // Metadados
    std::string texto(tamanhoMeta, '\0');
    if (!posicionar(h, deslocamentoMeta) || !lerExato(h, &texto[0], tamanhoMeta) ||
        Inflate::crc32(reinterpret_cast<const std::uint8_t *>(texto.data()), texto.size()) != crcMeta) {
        erro = "O instalador está corrompido (metadados). Baixe-o de novo.";
        return false;
    }
    std::size_t inicio = 0;
    while (inicio < texto.size()) {
        std::size_t fim = texto.find('\n', inicio);
        if (fim == std::string::npos)
            fim = texto.size();
        std::string linha = texto.substr(inicio, fim - inicio);
        if (!linha.empty() && linha.back() == '\r')
            linha.pop_back();
        const std::size_t igual = linha.find('=');
        if (igual != std::string::npos)
            pacote.meta[linha.substr(0, igual)] = linha.substr(igual + 1);
        inicio = fim + 1;
    }

    // Índice: percorre as entradas, pulando os dados.
    std::uint64_t posicao = pacote.tamanhoDoStub;
    std::uint64_t somaDescomprimida = 0;
    for (std::uint32_t i = 0; i < quantidade; ++i) {
        std::uint8_t cab[2];
        if (posicao + 2 > deslocamentoMeta || !posicionar(h, posicao) || !lerExato(h, cab, 2)) {
            erro = "O instalador está corrompido (índice).";
            return false;
        }
        const std::uint32_t tamanhoDoCaminho = static_cast<std::uint32_t>(le(cab, 2));
        std::vector<std::uint8_t> bloco(tamanhoDoCaminho + 21);
        if (tamanhoDoCaminho == 0 || tamanhoDoCaminho > 400 || posicao + 2 + bloco.size() > deslocamentoMeta || !lerExato(h, bloco.data(), bloco.size())) {
            erro = "O instalador está corrompido (índice).";
            return false;
        }
        Entrada e;
        e.caminho.assign(reinterpret_cast<const char *>(bloco.data()), tamanhoDoCaminho);
        const std::uint8_t *p = bloco.data() + tamanhoDoCaminho;
        e.tamanho = le(p, 8);
        e.tamanhoComprimido = le(p + 8, 8);
        e.crc = static_cast<std::uint32_t>(le(p + 16, 4));
        e.metodo = p[20];
        e.deslocamento = posicao + 2 + bloco.size();
        if (e.tamanho > kMaiorArquivo || e.tamanhoComprimido > kMaiorArquivo || (e.metodo != 0 && e.metodo != 1) ||
            e.deslocamento + e.tamanhoComprimido > deslocamentoMeta) {
            erro = "O instalador está corrompido (entrada inválida).";
            return false;
        }
        posicao = e.deslocamento + e.tamanhoComprimido;
        somaDescomprimida += e.tamanho;
        pacote.entradas.push_back(std::move(e));
    }
    if (posicao != deslocamentoMeta || somaDescomprimida != pacote.totalDescomprimido) {
        erro = "O instalador está corrompido (tamanhos não conferem).";
        return false;
    }
    return true;
}

bool lerEntrada(HANDLE arquivo, const Entrada &entrada, std::vector<std::uint8_t> &dados, std::string &erro)
{
    std::vector<std::uint8_t> comprimido(static_cast<std::size_t>(entrada.tamanhoComprimido));
    if (!posicionar(arquivo, entrada.deslocamento) || (!comprimido.empty() && !lerExato(arquivo, comprimido.data(), comprimido.size()))) {
        erro = "Não foi possível ler \"" + entrada.caminho + "\" do instalador.";
        return false;
    }
    dados.assign(static_cast<std::size_t>(entrada.tamanho), 0);
    if (entrada.metodo == 0) {
        if (comprimido.size() != dados.size()) {
            erro = "Dados inválidos em \"" + entrada.caminho + "\".";
            return false;
        }
        dados = std::move(comprimido);
    } else if (!Inflate::descomprimir(comprimido.data(), comprimido.size(), dados.data(), dados.size())) {
        erro = "O arquivo \"" + entrada.caminho + "\" está corrompido no instalador. Baixe-o de novo.";
        return false;
    }
    if (Inflate::crc32(dados.data(), dados.size()) != entrada.crc) {
        erro = "O arquivo \"" + entrada.caminho + "\" não confere com o original (CRC). Baixe o instalador de novo.";
        return false;
    }
    return true;
}

bool copiarInicio(const std::wstring &origem, const std::wstring &destino, std::uint64_t bytes, std::string &erro)
{
    HANDLE entrada = CreateFileW(origem.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (entrada == INVALID_HANDLE_VALUE) {
        erro = "Não foi possível ler o instalador.";
        return false;
    }
    HANDLE saida = CreateFileW(destino.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (saida == INVALID_HANDLE_VALUE) {
        CloseHandle(entrada);
        erro = "Não foi possível criar o desinstalador.";
        return false;
    }
    std::vector<std::uint8_t> buffer(1u << 16);
    bool ok = true;
    std::uint64_t restante = bytes;
    while (restante > 0 && ok) {
        const DWORD pedido = static_cast<DWORD>(restante > buffer.size() ? buffer.size() : restante);
        DWORD lido = 0, escrito = 0;
        ok = ReadFile(entrada, buffer.data(), pedido, &lido, nullptr) && lido == pedido && WriteFile(saida, buffer.data(), lido, &escrito, nullptr) && escrito == lido;
        restante -= pedido;
    }
    CloseHandle(entrada);
    CloseHandle(saida);
    if (!ok)
        erro = "Não foi possível gravar o desinstalador.";
    return ok;
}

}  // namespace Pacote
