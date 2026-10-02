#pragma once

#include "pacote.h"

#include <atomic>
#include <mutex>
#include <optional>
#include <string>

// A lógica de instalar e desinstalar (sem nenhuma interface): copiar arquivos, atalhos, registro, limpeza.
// Roda tanto em segundo plano (com a janela) quanto direto (modo silencioso: --silent).
namespace Instalar {

// Autoria (a mesma de src/core/BuildInfo.h): rodapé do instalador, "Editor" em Configurações > Aplicativos.
inline constexpr const wchar_t *kAutor = L"Gabriel (novakzx)";

struct Opcoes {
    std::wstring pasta;  // já resolvida (veja resolverPasta)
    bool atalhoAreaDeTrabalho = true;
    bool abrirAoTerminar = true;
    std::wstring atalhosEm;  // só para testes: grava os atalhos nesta pasta em vez do Menu Iniciar / área de trabalho
};

// Andamento, lido pela janela enquanto a instalação roda em outra thread.
struct Andamento {
    std::atomic<int> milesimos{0};  // 0..1000
    std::atomic<bool> terminou{false};
    std::atomic<bool> ok{false};

    void definirArquivo(const std::wstring &nome)
    {
        std::lock_guard<std::mutex> g(trava);
        arquivoAtual = nome;
    }
    std::wstring arquivo()
    {
        std::lock_guard<std::mutex> g(trava);
        return arquivoAtual;
    }
    void definirErro(const std::wstring &texto)
    {
        std::lock_guard<std::mutex> g(trava);
        erro = texto;
    }
    std::wstring mensagemDeErro()
    {
        std::lock_guard<std::mutex> g(trava);
        return erro;
    }

private:
    std::mutex trava;
    std::wstring arquivoAtual;
    std::wstring erro;
};

struct Existente {
    std::wstring pasta;
    std::wstring versao;
};

// Nomes fixos dentro da pasta de instalação.
extern const wchar_t *const kDesinstalador;  // Desinstalar.exe
extern const wchar_t *const kLista;          // desinstalar.lst (todos os arquivos instalados)

// Instalação já feita (lida do registro)? Só vale se o programa ainda estiver lá.
std::optional<Existente> detectar();
std::wstring pastaPadrao();
// Se a pasta escolhida já tem outras coisas (e não é uma instalação nossa), usa uma subpasta "Caderno+" dentro dela.
std::wstring resolverPasta(const std::wstring &escolhida);
// Mensagem de erro (vazia = pasta aceitável): precisa ser caminho absoluto, não ser a raiz do disco nem pastas do sistema.
std::wstring validarPasta(const std::wstring &pasta);

std::wstring pastaDosDados();  // %APPDATA%\ProfOrganizer

bool aplicativoAberto(const std::wstring &pasta, const std::wstring &nomeDoExe);

bool instalar(const Pacote::Conteudo &pacote, const Opcoes &opcoes, Andamento &andamento);
bool desinstalar(const std::wstring &pasta, bool apagarDados, Andamento &andamento, const std::wstring &atalhosEm = std::wstring());

}  // namespace Instalar
