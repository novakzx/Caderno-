// Instalador e desinstalador do Caderno+ (um único .exe, sem dependências).
//
//   CadernoSetup.exe                          abre o instalador (janela)
//   CadernoSetup.exe --silent [--dir PASTA] [--no-desktop] [--run]
//                                             instala sem janela (usado pelo teste automático e por quem quiser)
//   Desinstalar.exe  [--silent] [--delete-data]
//                                             remove o programa (por padrão, os dados do usuário ficam)
//   --capture PASTA                           grava um PNG de cada tela (para conferir o visual)
//
// Códigos de saída: 0 ok · 1 erro na janela · 2 argumentos inválidos · 3 pacote corrompido · 4 falha ao instalar/remover.

#include <windows.h>

#include <shellapi.h>

#include "instalar.h"
#include "janela.h"
#include "pacote.h"
#include "sistema.h"

#include <string>
#include <vector>

namespace {

struct Argumentos {
    bool silencioso = false;
    bool semAtalho = false;
    bool executarNoFim = false;
    bool apagarDados = false;
    bool invalido = false;
    std::wstring pasta;
    std::wstring capturar;
    std::wstring desinstalarDe;  // uso interno: o desinstalador copiado para a pasta temporária
    std::wstring atalhosEm;      // só para testes: pasta onde gravar os atalhos
};

Argumentos lerArgumentos()
{
    Argumentos a;
    int n = 0;
    LPWSTR *v = CommandLineToArgvW(GetCommandLineW(), &n);
    if (!v)
        return a;
    for (int i = 1; i < n; ++i) {
        const std::wstring arg = v[i];
        auto proximo = [&](std::wstring &destino) {
            if (i + 1 < n)
                destino = v[++i];
            else
                a.invalido = true;
        };
        if (arg == L"--silent" || arg == L"/S" || arg == L"/s" || arg == L"/silent" || arg == L"/VERYSILENT")
            a.silencioso = true;
        else if (arg == L"--no-desktop")
            a.semAtalho = true;
        else if (arg == L"--run")
            a.executarNoFim = true;
        else if (arg == L"--delete-data")
            a.apagarDados = true;
        else if (arg == L"--dir" || arg == L"/DIR")
            proximo(a.pasta);
        else if (arg == L"--capture")
            proximo(a.capturar);
        else if (arg == L"--uninstall-from")
            proximo(a.desinstalarDe);
        else if (arg == L"--shortcuts-in")
            proximo(a.atalhosEm);
        else
            a.invalido = true;
    }
    LocalFree(v);
    return a;
}

bool g_silencioso = false;  // no modo silencioso (CI, scripts) nunca abre caixa de mensagem: só devolve o código de saída

void mensagem(const std::wstring &texto, UINT icone = MB_ICONERROR)
{
    if (!g_silencioso)
        MessageBoxW(nullptr, texto.c_str(), L"Caderno+", MB_OK | icone);
}

// O desinstalador não pode apagar a si mesmo enquanto roda: depois de terminar, pede a um cmd para apagar a cópia temporária.
// `removerPasta` (opcional): depois de apagar o arquivo, tenta remover a pasta (o rmdir só funciona se ela estiver vazia).
void apagarEsteArquivoDepois(const std::wstring &removerPasta = std::wstring())
{
    const std::wstring eu = Sistema::caminhoDoExecutavel();
    std::wstring comando = L"cmd.exe /c ping 127.0.0.1 -n 3 >nul & del /f /q \"" + eu + L"\"";
    if (!removerPasta.empty())
        comando += L" & rmdir \"" + removerPasta + L"\"";
    std::wstring linha = comando;
    STARTUPINFOW si = {};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi = {};
    if (CreateProcessW(nullptr, &linha[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

Janela::Configuracao configuracaoDeInstalacao(const Pacote::Conteudo &pacote, const std::wstring &pastaEscolhida)
{
    Janela::Configuracao c;
    c.modo = Janela::Modo::Instalar;
    c.pacote = pacote;
    c.versao = Sistema::paraLargo(pacote.valor("versao", ""));
    c.pasta = pastaEscolhida.empty() ? Instalar::pastaPadrao() : pastaEscolhida;
    c.tamanhoEmMB = (pacote.totalDescomprimido + (1u << 20) - 1) >> 20;
    if (const auto existente = Instalar::detectar()) {
        c.jaInstalado = true;
        c.versaoInstalada = existente->versao;
        c.pasta = pastaEscolhida.empty() ? existente->pasta : pastaEscolhida;
    }
    const std::wstring exe = Sistema::paraLargo(pacote.valor("exe", "ProfOrganizer.exe"));
    c.aplicativoAberto = Instalar::aplicativoAberto(c.pasta, exe);
    return c;
}

int instalarSemJanela(const Pacote::Conteudo &pacote, const Argumentos &a)
{
    Instalar::Opcoes opcoes;
    opcoes.pasta = Instalar::resolverPasta(a.pasta.empty() ? Instalar::pastaPadrao() : a.pasta);
    opcoes.atalhoAreaDeTrabalho = !a.semAtalho;
    opcoes.atalhosEm = a.atalhosEm;
    Instalar::Andamento andamento;
    if (!Instalar::instalar(pacote, opcoes, andamento))
        return 4;
    if (a.executarNoFim)
        Sistema::executar(Sistema::juntar(opcoes.pasta, Sistema::paraLargo(pacote.valor("exe", "ProfOrganizer.exe"))), L"", opcoes.pasta, false);
    return 0;
}

int desinstalar(HINSTANCE instancia, const std::wstring &pasta, const Argumentos &a)
{
    int codigo = 0;
    if (a.silencioso) {
        Instalar::Andamento andamento;
        codigo = Instalar::desinstalar(pasta, a.apagarDados, andamento, a.atalhosEm) ? 0 : 4;
    } else {
        Janela::Configuracao c;
        c.modo = Janela::Modo::Desinstalar;
        c.pasta = pasta;
        if (const auto existente = Instalar::detectar())
            c.versao = existente->versao;
        codigo = Janela::executar(instancia, c);
    }
    apagarEsteArquivoDepois();
    return codigo;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instancia, HINSTANCE, PWSTR, int)
{
    const Argumentos a = lerArgumentos();
    g_silencioso = a.silencioso;
    if (a.invalido) {
        mensagem(L"Opção desconhecida.\n\nInstalar: CadernoSetup.exe [--silent] [--dir PASTA] [--no-desktop] [--run]\nRemover: Desinstalar.exe [--silent] [--delete-data]");
        return 2;
    }

    const std::wstring eu = Sistema::caminhoDoExecutavel();

    // Cópia temporária do desinstalador, já em execução fora da pasta que vai ser apagada.
    if (!a.desinstalarDe.empty())
        return desinstalar(instancia, a.desinstalarDe, a);

    Pacote::Conteudo pacote;
    std::string erro;
    const bool temPacote = Pacote::abrir(eu, pacote, erro);

    if (!temPacote) {
        if (erro != "sem pacote") {
            mensagem(Sistema::paraLargo(erro));
            return 3;
        }
        // Sem pacote = este é o Desinstalar.exe da pasta de instalação. Copia a si mesmo para a pasta temporária e
        // continua de lá (para poder apagar a pasta inteira, inclusive este arquivo).
        const std::wstring pasta = Sistema::pastaDoArquivo(eu);
        const std::wstring copia = Sistema::juntar(Sistema::pastaTemporaria(), L"CadernoDesinstalar-" + std::to_wstring(GetCurrentProcessId()) + L".exe");
        if (!CopyFileW(eu.c_str(), copia.c_str(), FALSE)) {
            mensagem(L"Não foi possível preparar a remoção (pasta temporária indisponível).");
            return 4;
        }
        std::wstring argumentos = L"--uninstall-from \"" + pasta + L"\"";
        if (a.silencioso)
            argumentos += L" --silent";
        if (a.apagarDados)
            argumentos += L" --delete-data";
        if (!a.atalhosEm.empty())
            argumentos += L" --shortcuts-in \"" + a.atalhosEm + L"\"";
        std::wstring linha = L"\"" + copia + L"\" " + argumentos;
        STARTUPINFOW si = {};
        si.cb = sizeof si;
        PROCESS_INFORMATION pi = {};
        if (!CreateProcessW(nullptr, &linha[0], nullptr, nullptr, FALSE, a.silencioso ? CREATE_NO_WINDOW : 0, nullptr, nullptr, &si, &pi)) {
            mensagem(L"Não foi possível iniciar a remoção.");
            return 4;
        }
        DWORD codigo = 0;
        if (a.silencioso) {  // no modo silencioso, espera terminar para devolver o resultado de verdade
            WaitForSingleObject(pi.hProcess, 10 * 60 * 1000);
            GetExitCodeProcess(pi.hProcess, &codigo);
            // Este arquivo está em uso enquanto roda: pede a um cmd para apagá-lo (e a pasta, se ficar vazia) logo depois.
            apagarEsteArquivoDepois(pasta);
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return static_cast<int>(codigo);
    }

    // Modo instalador
    if (!a.capturar.empty()) {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        Janela::Configuracao c = configuracaoDeInstalacao(pacote, a.pasta);
        const bool ok = Janela::capturar(instancia, c, a.capturar);
        Janela::Configuracao d = c;
        d.modo = Janela::Modo::Desinstalar;
        const bool ok2 = Janela::capturar(instancia, d, Sistema::juntar(a.capturar, L"desinstalar"));
        return ok && ok2 ? 0 : 1;
    }
    if (a.silencioso)
        return instalarSemJanela(pacote, a);

    const Janela::Configuracao c = configuracaoDeInstalacao(pacote, a.pasta);
    return Janela::executar(instancia, c);
}
