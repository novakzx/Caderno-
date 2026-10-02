#include "instalar.h"

#include "sistema.h"

#include <shlobj.h>

#include <algorithm>
#include <set>
#include <vector>

namespace Instalar {

const wchar_t *const kDesinstalador = L"Desinstalar.exe";
const wchar_t *const kLista = L"desinstalar.lst";

namespace {

const wchar_t *const kChaveDesinstalar = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CadernoPlus";
// Instalador antigo (Inno Setup) das versões anteriores: se existir, é substituído por este.
const wchar_t *const kChaveInno = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{6F1B7C52-3A4E-4D0B-9C61-2E7A5B8D4C13}_is1";
const wchar_t *const kChaveConfiguracoes = L"Software\\ProfOrganizer";  // QSettings do programa
const char *const kCabecalhoDaLista = "CADERNO-LST1";

std::wstring minusculas(std::wstring s)
{
    for (wchar_t &c : s)
        c = static_cast<wchar_t>(towlower(c));
    return s;
}

std::wstring comBarraInvertida(std::wstring s)
{
    std::replace(s.begin(), s.end(), L'/', L'\\');
    return s;
}

std::wstring nomeDoArquivo(const std::wstring &caminho)
{
    return caminho.substr(caminho.find_last_of(L"\\/") + 1);
}

// Lê a lista de arquivos de uma instalação anterior (vazia se não houver/for inválida).
std::vector<std::wstring> lerLista(const std::wstring &pasta)
{
    std::vector<std::wstring> arquivos;
    std::string texto;
    if (!Sistema::lerArquivoTexto(Sistema::juntar(pasta, kLista), &texto))
        return arquivos;
    size_t inicio = 0;
    bool primeira = true;
    while (inicio < texto.size()) {
        size_t fim = texto.find('\n', inicio);
        if (fim == std::string::npos)
            fim = texto.size();
        std::string linha = texto.substr(inicio, fim - inicio);
        if (!linha.empty() && linha.back() == '\r')
            linha.pop_back();
        inicio = fim + 1;
        if (primeira) {
            primeira = false;
            if (linha != kCabecalhoDaLista)
                return {};  // não é uma lista nossa
            continue;
        }
        const std::wstring rel = comBarraInvertida(Sistema::paraLargo(linha));
        if (Sistema::caminhoRelativoSeguro(rel))
            arquivos.push_back(rel);
    }
    return arquivos;
}

// Remove as pastas vazias que ficaram para trás (das mais profundas para as mais rasas), sem tocar nas que têm outras coisas.
void limparPastasVazias(const std::wstring &raiz, const std::vector<std::wstring> &arquivos)
{
    std::set<std::wstring> pastas;
    for (const std::wstring &rel : arquivos) {
        std::wstring atual = Sistema::pastaDoArquivo(rel);
        while (!atual.empty()) {
            pastas.insert(atual);
            atual = Sistema::pastaDoArquivo(atual);
        }
    }
    std::vector<std::wstring> ordenadas(pastas.begin(), pastas.end());
    std::sort(ordenadas.begin(), ordenadas.end(), [](const std::wstring &a, const std::wstring &b) { return a.size() > b.size(); });
    for (const std::wstring &p : ordenadas)
        Sistema::apagarPastaSeVazia(Sistema::juntar(raiz, p));
}

bool moverParaOLugar(const std::wstring &temporario, const std::wstring &destino)
{
    // Se o arquivo antigo estiver em uso por um instante (antivírus, por exemplo), tenta de novo algumas vezes.
    for (int tentativa = 0; tentativa < 6; ++tentativa) {
        if (MoveFileExW(temporario.c_str(), destino.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return true;
        Sleep(250);
    }
    return false;
}

// `em` vazio = os lugares de verdade (Menu Iniciar e área de trabalho do usuário).
std::wstring caminhoDoAtalhoNoMenu(const std::wstring &em)
{
    return em.empty() ? Sistema::juntar(Sistema::pastaConhecida(FOLDERID_Programs), L"Caderno+.lnk") : Sistema::juntar(em, L"Menu Iniciar - Caderno+.lnk");
}

std::wstring caminhoDoAtalhoNaArea(const std::wstring &em)
{
    return em.empty() ? Sistema::juntar(Sistema::pastaConhecida(FOLDERID_Desktop), L"Caderno+.lnk") : Sistema::juntar(em, L"Area de trabalho - Caderno+.lnk");
}

}  // namespace

std::wstring pastaDosDados()
{
    return Sistema::juntar(Sistema::pastaConhecida(FOLDERID_RoamingAppData), L"ProfOrganizer");
}

std::optional<Existente> detectar()
{
    Existente e;
    if (!Sistema::lerTexto(kChaveDesinstalar, L"InstallLocation", &e.pasta) || e.pasta.empty())
        return std::nullopt;
    if (!Sistema::existe(Sistema::juntar(e.pasta, kDesinstalador)) && !Sistema::existe(Sistema::juntar(e.pasta, L"ProfOrganizer.exe")))
        return std::nullopt;
    Sistema::lerTexto(kChaveDesinstalar, L"DisplayVersion", &e.versao);
    return e;
}

std::wstring pastaPadrao()
{
    if (const auto existente = detectar())
        return existente->pasta;
    // Versão antiga (Inno Setup): reaproveita a mesma pasta.
    std::wstring antiga;
    if (Sistema::lerTexto(kChaveInno, L"InstallLocation", &antiga) && !antiga.empty()) {
        while (!antiga.empty() && (antiga.back() == L'\\' || antiga.back() == L'/'))
            antiga.pop_back();
        return antiga;
    }
    return Sistema::juntar(Sistema::juntar(Sistema::pastaConhecida(FOLDERID_LocalAppData), L"Programs"), L"Caderno+");
}

std::wstring validarPasta(const std::wstring &pasta)
{
    if (pasta.size() < 4 || pasta[1] != L':' || (pasta[2] != L'\\' && pasta[2] != L'/'))
        return L"Escolha uma pasta completa, como C:\\Programas\\Caderno+.";
    std::wstring limpa = pasta;
    while (limpa.size() > 3 && (limpa.back() == L'\\' || limpa.back() == L'/'))
        limpa.pop_back();
    if (limpa.size() <= 3)
        return L"Não instale na raiz do disco. Escolha uma pasta.";
    const std::wstring baixa = minusculas(limpa);
    for (const GUID *id : {&FOLDERID_Windows, &FOLDERID_ProgramFiles, &FOLDERID_ProgramFilesX86, &FOLDERID_System, &FOLDERID_Profile, &FOLDERID_Desktop, &FOLDERID_Documents}) {
        std::wstring proibida = minusculas(Sistema::pastaConhecida(*id));
        while (!proibida.empty() && (proibida.back() == L'\\' || proibida.back() == L'/'))
            proibida.pop_back();
        if (!proibida.empty() && baixa == proibida)
            return L"Essa pasta é do sistema. Escolha uma subpasta (por exemplo, dentro dela, \"Caderno+\").";
    }
    if (baixa.find(L"..") != std::wstring::npos)
        return L"O caminho da pasta não pode conter \"..\".";
    return std::wstring();
}

std::wstring resolverPasta(const std::wstring &escolhida)
{
    std::wstring pasta = escolhida;
    while (pasta.size() > 3 && (pasta.back() == L'\\' || pasta.back() == L'/'))
        pasta.pop_back();
    // Pasta que já tem outras coisas e não é uma instalação nossa: não mistura, usa uma subpasta.
    if (Sistema::existe(pasta) && !Sistema::ehPastaVazia(pasta) && !Sistema::existe(Sistema::juntar(pasta, kLista)) &&
        !Sistema::existe(Sistema::juntar(pasta, L"ProfOrganizer.exe"))) {
        const std::wstring nome = minusculas(nomeDoArquivo(pasta));
        if (nome != L"caderno+")
            return Sistema::juntar(pasta, L"Caderno+");
    }
    return pasta;
}

bool aplicativoAberto(const std::wstring &pasta, const std::wstring &nomeDoExe)
{
    return !Sistema::processosDoExecutavel(Sistema::juntar(pasta, nomeDoExe)).empty();
}

// ---------------------------------------------------------------- instalar

bool instalar(const Pacote::Conteudo &pacote, const Opcoes &opcoes, Andamento &andamento)
{
    auto falhar = [&](const std::wstring &mensagem) {
        andamento.definirErro(mensagem);
        andamento.ok = false;
        andamento.terminou = true;
        return false;
    };

    const std::wstring pasta = opcoes.pasta;
    const std::wstring nomeDoExe = Sistema::paraLargo(pacote.valor("exe", "ProfOrganizer.exe"));
    const std::wstring versao = Sistema::paraLargo(pacote.valor("versao", ""));

    if (const std::wstring problema = validarPasta(pasta); !problema.empty())
        return falhar(problema);

    // 0) Antes de gravar QUALQUER coisa, confere todos os caminhos do pacote: nada de "..", unidade, nome reservado.
    for (const Pacote::Entrada &entrada : pacote.entradas) {
        const std::wstring rel = comBarraInvertida(Sistema::paraLargo(entrada.caminho));
        const std::wstring baixa = minusculas(rel);
        if (!Sistema::caminhoRelativoSeguro(rel) || baixa == minusculas(kDesinstalador) || baixa == minusculas(kLista))
            return falhar(L"O instalador tem um caminho de arquivo inválido (\"" + rel + L"\"). Nada foi instalado. Baixe-o de novo.");
    }

    // 1) Espaço em disco
    const unsigned long long livre = Sistema::espacoLivre(pasta);
    const unsigned long long preciso = pacote.totalDescomprimido + (64ull << 20);
    if (livre != 0 && livre < preciso)
        return falhar(L"Não há espaço suficiente no disco. São necessários cerca de " + std::to_wstring(preciso >> 20) + L" MB livres.");

    // 2) Pasta de destino
    if (!Sistema::criarPastas(pasta))
        return falhar(L"Não foi possível criar a pasta \"" + pasta + L"\". Escolha outra pasta ou verifique as permissões.");

    // 3) Se o programa estiver aberto, pede para fechar (ele salva o que estiver editando ao fechar).
    andamento.definirArquivo(L"Fechando o Caderno+…");
    const std::vector<DWORD> abertos = Sistema::processosDoExecutavel(Sistema::juntar(pasta, nomeDoExe));
    if (!abertos.empty() && !Sistema::fecharProcessos(abertos, 12000))
        return falhar(L"O Caderno+ está aberto e não fechou sozinho. Feche-o e tente de novo.");

    const std::vector<std::wstring> listaAntiga = lerLista(pasta);

    // 4) Arquivos
    HANDLE origem = CreateFileW(pacote.arquivo.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (origem == INVALID_HANDLE_VALUE)
        return falhar(L"Não foi possível ler o instalador.");
    struct Fechar {
        HANDLE h;
        ~Fechar() { CloseHandle(h); }
    } fechar{origem};

    std::vector<std::wstring> instalados;
    std::set<std::wstring> instaladosEmMinusculas;
    std::uint64_t gravados = 0;
    const std::uint64_t total = pacote.totalDescomprimido ? pacote.totalDescomprimido : 1;
    std::vector<std::uint8_t> dados;
    for (const Pacote::Entrada &entrada : pacote.entradas) {
        const std::wstring rel = comBarraInvertida(Sistema::paraLargo(entrada.caminho));
        const std::wstring baixa = minusculas(rel);
        if (!Sistema::caminhoRelativoSeguro(rel) || baixa == minusculas(kDesinstalador) || baixa == minusculas(kLista))
            return falhar(L"O instalador tem um caminho de arquivo inválido (\"" + rel + L"\"). Baixe-o de novo.");
        andamento.definirArquivo(rel);

        std::string erro;
        if (!Pacote::lerEntrada(origem, entrada, dados, erro))
            return falhar(Sistema::paraLargo(erro));

        const std::wstring destino = Sistema::juntar(pasta, rel);
        if (!Sistema::criarPastas(Sistema::pastaDoArquivo(destino)))
            return falhar(L"Não foi possível criar a pasta de \"" + rel + L"\".");
        const std::wstring temporario = destino + L".novo";
        std::wstring erroDeEscrita;
        if (!Sistema::escreverArquivo(temporario, dados.data(), dados.size(), &erroDeEscrita)) {
            Sistema::apagarArquivo(temporario);
            return falhar(L"Não foi possível gravar \"" + rel + L"\" (" + erroDeEscrita + L"). O disco pode estar cheio ou a pasta protegida.");
        }
        if (!moverParaOLugar(temporario, destino)) {
            Sistema::apagarArquivo(temporario);
            return falhar(L"Não foi possível substituir \"" + rel + L"\". Ele pode estar aberto em outro programa.");
        }
        instalados.push_back(rel);
        instaladosEmMinusculas.insert(baixa);
        gravados += entrada.tamanho;
        andamento.milesimos = static_cast<int>(900 * gravados / total);
    }

    // 5) Arquivos da versão anterior que não existem mais nesta
    for (const std::wstring &velho : listaAntiga) {
        if (instaladosEmMinusculas.count(minusculas(velho)) == 0)
            Sistema::apagarArquivo(Sistema::juntar(pasta, velho));
    }
    limparPastasVazias(pasta, listaAntiga);

    // 6) Desinstalador, lista de arquivos e versão
    andamento.definirArquivo(L"Finalizando…");
    const std::wstring desinstalador = Sistema::juntar(pasta, kDesinstalador);
    {
        std::string erro;
        const std::wstring temporario = desinstalador + L".novo";
        if (!Pacote::copiarInicio(pacote.arquivo, temporario, pacote.tamanhoDoStub, erro))
            return falhar(Sistema::paraLargo(erro));
        if (!moverParaOLugar(temporario, desinstalador)) {
            Sistema::apagarArquivo(temporario);
            return falhar(L"Não foi possível criar o desinstalador.");
        }
    }
    {
        std::string lista = std::string(kCabecalhoDaLista) + "\n";
        for (const std::wstring &rel : instalados)
            lista += Sistema::paraUtf8(rel) + "\n";
        std::wstring erro;
        if (!Sistema::escreverArquivo(Sistema::juntar(pasta, kLista), lista.data(), lista.size(), &erro))
            return falhar(L"Não foi possível gravar a lista de arquivos (" + erro + L").");
    }
    andamento.milesimos = 930;

    // 7) Atalhos
    const std::wstring exe = Sistema::juntar(pasta, nomeDoExe);
    if (!opcoes.atalhosEm.empty())
        Sistema::criarPastas(opcoes.atalhosEm);
    Sistema::criarAtalho(caminhoDoAtalhoNoMenu(opcoes.atalhosEm), exe, pasta, L"Caderno+ — organizador para professores");
    if (opcoes.atalhoAreaDeTrabalho)
        Sistema::criarAtalho(caminhoDoAtalhoNaArea(opcoes.atalhosEm), exe, pasta, L"Caderno+ — organizador para professores");
    andamento.milesimos = 960;

    // 8) Registro (Configurações > Aplicativos)
    const std::wstring chave = kChaveDesinstalar;
    Sistema::gravarTexto(chave, L"DisplayName", L"Caderno+");
    Sistema::gravarTexto(chave, L"DisplayVersion", versao);
    Sistema::gravarTexto(chave, L"Publisher", L"Caderno+");
    Sistema::gravarTexto(chave, L"InstallLocation", pasta);
    Sistema::gravarTexto(chave, L"DisplayIcon", exe);
    Sistema::gravarTexto(chave, L"UninstallString", L"\"" + desinstalador + L"\"");
    Sistema::gravarTexto(chave, L"QuietUninstallString", L"\"" + desinstalador + L"\" --silent");
    Sistema::gravarTexto(chave, L"URLInfoAbout", L"https://github.com/novakzx/Caderno-");
    Sistema::gravarNumero(chave, L"NoModify", 1);
    Sistema::gravarNumero(chave, L"NoRepair", 1);
    Sistema::gravarNumero(chave, L"EstimatedSize", static_cast<DWORD>(pacote.totalDescomprimido >> 10));

    // 9) Instalação antiga feita com o Inno Setup: este instalador passa a ser o dono da pasta.
    if (Sistema::existe(Sistema::juntar(pasta, L"unins000.exe"))) {
        Sistema::apagarArquivo(Sistema::juntar(pasta, L"unins000.exe"));
        Sistema::apagarArquivo(Sistema::juntar(pasta, L"unins000.dat"));
        Sistema::apagarChave(kChaveInno);
    }

    andamento.definirArquivo(L"");
    andamento.milesimos = 1000;
    andamento.ok = true;
    andamento.terminou = true;
    return true;
}

// ------------------------------------------------------------- desinstalar

bool desinstalar(const std::wstring &pasta, bool apagarDados, Andamento &andamento, const std::wstring &atalhosEm)
{
    auto falhar = [&](const std::wstring &mensagem) {
        andamento.definirErro(mensagem);
        andamento.ok = false;
        andamento.terminou = true;
        return false;
    };

    const std::vector<std::wstring> arquivos = lerLista(pasta);
    if (arquivos.empty())
        return falhar(L"Não encontrei a lista de arquivos desta instalação (" + pasta + L"). Nada foi removido.");

    // Fecha o programa, se estiver aberto.
    andamento.definirArquivo(L"Fechando o Caderno+…");
    std::wstring nomeDoExe = L"ProfOrganizer.exe";
    const std::vector<DWORD> abertos = Sistema::processosDoExecutavel(Sistema::juntar(pasta, nomeDoExe));
    if (!abertos.empty() && !Sistema::fecharProcessos(abertos, 12000))
        return falhar(L"O Caderno+ está aberto e não fechou sozinho. Feche-o e tente de novo.");

    const size_t total = arquivos.size() + 1;
    size_t feitos = 0;
    bool algumFalhou = false;
    for (const std::wstring &rel : arquivos) {
        andamento.definirArquivo(rel);
        if (!Sistema::apagarArquivo(Sistema::juntar(pasta, rel)))
            algumFalhou = true;
        ++feitos;
        andamento.milesimos = static_cast<int>(800 * feitos / total);
    }
    limparPastasVazias(pasta, arquivos);

    // Atalhos (só se forem nossos: têm o nome certo e apontam para o nosso programa; aqui basta o nome)
    Sistema::apagarArquivo(caminhoDoAtalhoNoMenu(atalhosEm));
    Sistema::apagarArquivo(caminhoDoAtalhoNaArea(atalhosEm));
    andamento.milesimos = 850;

    Sistema::apagarChave(kChaveDesinstalar);

    if (apagarDados) {
        andamento.definirArquivo(L"Apagando os dados…");
        const std::wstring dados = pastaDosDados();
        // Trava de segurança: só apaga uma pasta chamada exatamente "ProfOrganizer".
        if (!dados.empty() && minusculas(nomeDoArquivo(dados)) == L"proforganizer" && Sistema::existe(dados))
            Sistema::apagarArvore(dados);
        Sistema::apagarChave(kChaveConfiguracoes);
    }
    andamento.milesimos = 950;

    // Por último: lista, desinstalador e a pasta (se ficar vazia).
    Sistema::apagarArquivo(Sistema::juntar(pasta, kLista));
    Sistema::apagarArquivo(Sistema::juntar(pasta, kDesinstalador));
    Sistema::apagarPastaSeVazia(pasta);

    andamento.definirArquivo(L"");
    andamento.milesimos = 1000;
    if (algumFalhou) {
        andamento.definirErro(L"Alguns arquivos não puderam ser removidos (estão em uso). Reinicie o computador e apague a pasta \"" + pasta + L"\".");
        andamento.ok = false;
        andamento.terminou = true;
        return false;
    }
    andamento.ok = true;
    andamento.terminou = true;
    return true;
}

}  // namespace Instalar
