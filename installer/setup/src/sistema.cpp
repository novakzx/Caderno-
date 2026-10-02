#include "sistema.h"

#include <objbase.h>
#include <shellapi.h>
#include <shlguid.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cwctype>

namespace Sistema {

// ------------------------------------------------------------------ Texto

std::wstring paraLargo(const std::string &utf8)
{
    if (utf8.empty())
        return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring saida(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), &saida[0], n);
    return saida;
}

std::string paraUtf8(const std::wstring &largo)
{
    if (largo.empty())
        return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, largo.data(), static_cast<int>(largo.size()), nullptr, 0, nullptr, nullptr);
    std::string saida(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, largo.data(), static_cast<int>(largo.size()), &saida[0], n, nullptr, nullptr);
    return saida;
}

// ---------------------------------------------------------------- Caminhos

std::wstring caminhoDoExecutavel()
{
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, &buf[0], static_cast<DWORD>(buf.size()));
        if (n == 0)
            return std::wstring();
        if (n < buf.size()) {
            buf.resize(n);
            return buf;
        }
        buf.resize(buf.size() * 2);
    }
}

std::wstring pastaDoArquivo(const std::wstring &caminho)
{
    const size_t i = caminho.find_last_of(L"\\/");
    return i == std::wstring::npos ? std::wstring() : caminho.substr(0, i);
}

std::wstring juntar(const std::wstring &a, const std::wstring &b)
{
    if (a.empty())
        return b;
    if (a.back() == L'\\' || a.back() == L'/')
        return a + b;
    return a + L'\\' + b;
}

std::wstring pastaConhecida(const GUID &id)
{
    PWSTR caminho = nullptr;
    std::wstring saida;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &caminho)) && caminho)
        saida = caminho;
    CoTaskMemFree(caminho);
    return saida;
}

std::wstring pastaTemporaria()
{
    wchar_t buf[MAX_PATH + 2];
    const DWORD n = GetTempPathW(MAX_PATH + 1, buf);
    std::wstring saida(buf, n);
    while (!saida.empty() && (saida.back() == L'\\' || saida.back() == L'/'))
        saida.pop_back();
    return saida;
}

bool existe(const std::wstring &caminho)
{
    return GetFileAttributesW(caminho.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool ehPastaVazia(const std::wstring &pasta)
{
    WIN32_FIND_DATAW dados;
    HANDLE h = FindFirstFileW(juntar(pasta, L"*").c_str(), &dados);
    if (h == INVALID_HANDLE_VALUE)
        return true;  // não existe ou não dá para listar
    bool vazia = true;
    do {
        if (wcscmp(dados.cFileName, L".") != 0 && wcscmp(dados.cFileName, L"..") != 0) {
            vazia = false;
            break;
        }
    } while (FindNextFileW(h, &dados));
    FindClose(h);
    return vazia;
}

bool criarPastas(const std::wstring &pasta)
{
    if (pasta.empty())
        return false;
    const DWORD atributos = GetFileAttributesW(pasta.c_str());
    if (atributos != INVALID_FILE_ATTRIBUTES)
        return (atributos & FILE_ATTRIBUTE_DIRECTORY) != 0;
    const std::wstring mae = pastaDoArquivo(pasta);
    if (!mae.empty() && mae != pasta && !criarPastas(mae))
        return false;
    return CreateDirectoryW(pasta.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool caminhoRelativoSeguro(const std::wstring &relativo)
{
    if (relativo.empty() || relativo.size() > 240)
        return false;
    if (relativo.front() == L'/' || relativo.front() == L'\\')
        return false;
    if (relativo.find(L':') != std::wstring::npos)  // unidade ("C:") ou fluxo alternativo do NTFS ("arq:fluxo")
        return false;
    // Cada trecho entre barras: nem vazio, nem "." / "..", nem terminado em ponto ou espaço.
    size_t inicio = 0;
    while (inicio <= relativo.size()) {
        size_t fim = relativo.find_first_of(L"\\/", inicio);
        if (fim == std::wstring::npos)
            fim = relativo.size();
        const std::wstring parte = relativo.substr(inicio, fim - inicio);
        if (parte.empty() || parte == L"." || parte == L".." || parte.back() == L'.' || parte.back() == L' ')
            return false;
        for (wchar_t c : parte)
            if (c < 32 || c == L'<' || c == L'>' || c == L'"' || c == L'|' || c == L'?' || c == L'*')
                return false;
        inicio = fim + 1;
    }
    return true;
}

// ---------------------------------------------------------------- Arquivos

bool escreverArquivo(const std::wstring &caminho, const void *dados, size_t tamanho, std::wstring *erro)
{
    HANDLE h = CreateFileW(caminho.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        if (erro)
            *erro = L"código " + std::to_wstring(GetLastError());
        return false;
    }
    const BYTE *p = static_cast<const BYTE *>(dados);
    size_t restante = tamanho;
    bool ok = true;
    while (restante > 0) {
        const DWORD pedaco = static_cast<DWORD>((std::min<size_t>)(restante, 1u << 20));
        DWORD escrito = 0;
        if (!WriteFile(h, p, pedaco, &escrito, nullptr) || escrito != pedaco) {
            ok = false;
            if (erro)
                *erro = L"código " + std::to_wstring(GetLastError());
            break;
        }
        p += pedaco;
        restante -= pedaco;
    }
    CloseHandle(h);
    return ok;
}

bool lerArquivoTexto(const std::wstring &caminho, std::string *conteudo)
{
    HANDLE h = CreateFileW(caminho.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER tamanho;
    if (!GetFileSizeEx(h, &tamanho) || tamanho.QuadPart > (16 << 20)) {  // listas passam de poucos KB; 16 MB é teto de sanidade
        CloseHandle(h);
        return false;
    }
    conteudo->assign(static_cast<size_t>(tamanho.QuadPart), '\0');
    DWORD lido = 0;
    const bool ok = tamanho.QuadPart == 0 || (ReadFile(h, &(*conteudo)[0], static_cast<DWORD>(tamanho.QuadPart), &lido, nullptr) && lido == tamanho.QuadPart);
    CloseHandle(h);
    return ok;
}

bool apagarArquivo(const std::wstring &caminho)
{
    SetFileAttributesW(caminho.c_str(), FILE_ATTRIBUTE_NORMAL);
    if (DeleteFileW(caminho.c_str()))
        return true;
    return GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND;
}

bool apagarPastaSeVazia(const std::wstring &pasta)
{
    return RemoveDirectoryW(pasta.c_str()) != 0;  // só funciona se estiver vazia
}

bool apagarArvore(const std::wstring &pasta)
{
    // SHFileOperation espera uma lista terminada por dois NULs.
    std::wstring origem = pasta;
    origem.push_back(L'\0');
    origem.push_back(L'\0');
    SHFILEOPSTRUCTW op = {};
    op.wFunc = FO_DELETE;
    op.pFrom = origem.c_str();
    op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    return SHFileOperationW(&op) == 0;
}

unsigned long long espacoLivre(const std::wstring &pasta)
{
    // Sobe até uma pasta que exista (a de destino ainda pode não ter sido criada).
    std::wstring atual = pasta;
    while (!atual.empty() && !existe(atual)) {
        const std::wstring mae = pastaDoArquivo(atual);
        if (mae == atual)
            break;
        atual = mae;
    }
    if (atual.empty())
        return 0;
    ULARGE_INTEGER livre = {};
    if (!GetDiskFreeSpaceExW(atual.c_str(), &livre, nullptr, nullptr))
        return 0;
    return livre.QuadPart;
}

// ----------------------------------------------------------------- Atalhos

bool criarAtalho(const std::wstring &caminhoDoLnk, const std::wstring &alvo, const std::wstring &pastaDeTrabalho, const std::wstring &descricao)
{
    const HRESULT inicio = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool ok = false;
    IShellLinkW *link = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void **>(&link))) && link) {
        link->SetPath(alvo.c_str());
        link->SetWorkingDirectory(pastaDeTrabalho.c_str());
        link->SetDescription(descricao.c_str());
        link->SetIconLocation(alvo.c_str(), 0);
        IPersistFile *arquivo = nullptr;
        if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&arquivo))) && arquivo) {
            ok = SUCCEEDED(arquivo->Save(caminhoDoLnk.c_str(), TRUE));
            arquivo->Release();
        }
        link->Release();
    }
    if (SUCCEEDED(inicio))
        CoUninitialize();
    return ok;
}

// ----------------------------------------------------------------- Registro

bool gravarTexto(const std::wstring &chave, const std::wstring &nome, const std::wstring &valor)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, chave.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS)
        return false;
    const LONG r = RegSetValueExW(k, nome.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE *>(valor.c_str()),
                                  static_cast<DWORD>((valor.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

bool gravarNumero(const std::wstring &chave, const std::wstring &nome, DWORD valor)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, chave.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS)
        return false;
    const LONG r = RegSetValueExW(k, nome.c_str(), 0, REG_DWORD, reinterpret_cast<const BYTE *>(&valor), sizeof valor);
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

bool lerTexto(const std::wstring &chave, const std::wstring &nome, std::wstring *valor)
{
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, chave.c_str(), 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS)
        return false;
    DWORD tipo = 0, tamanho = 0;
    bool ok = false;
    if (RegQueryValueExW(k, nome.c_str(), nullptr, &tipo, nullptr, &tamanho) == ERROR_SUCCESS && tipo == REG_SZ && tamanho >= sizeof(wchar_t)) {
        std::wstring buf(tamanho / sizeof(wchar_t), L'\0');
        if (RegQueryValueExW(k, nome.c_str(), nullptr, nullptr, reinterpret_cast<BYTE *>(&buf[0]), &tamanho) == ERROR_SUCCESS) {
            while (!buf.empty() && buf.back() == L'\0')
                buf.pop_back();
            *valor = buf;
            ok = true;
        }
    }
    RegCloseKey(k);
    return ok;
}

bool apagarChave(const std::wstring &chave)
{
    const LONG r = RegDeleteTreeW(HKEY_CURRENT_USER, chave.c_str());
    return r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND;
}

// ---------------------------------------------------------------- Processos

static bool mesmoCaminho(const std::wstring &a, const std::wstring &b)
{
    return CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()), b.c_str(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}

std::vector<DWORD> processosDoExecutavel(const std::wstring &caminhoDoExe)
{
    std::vector<DWORD> pids;
    HANDLE foto = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (foto == INVALID_HANDLE_VALUE)
        return pids;
    PROCESSENTRY32W p = {};
    p.dwSize = sizeof p;
    const std::wstring nomeDoArquivo = caminhoDoExe.substr(caminhoDoExe.find_last_of(L"\\/") + 1);
    for (BOOL ok = Process32FirstW(foto, &p); ok; ok = Process32NextW(foto, &p)) {
        if (!mesmoCaminho(p.szExeFile, nomeDoArquivo))
            continue;
        HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p.th32ProcessID);
        if (!proc)
            continue;
        wchar_t caminho[MAX_PATH * 2];
        DWORD n = MAX_PATH * 2;
        if (QueryFullProcessImageNameW(proc, 0, caminho, &n) && mesmoCaminho(std::wstring(caminho, n), caminhoDoExe))
            pids.push_back(p.th32ProcessID);
        CloseHandle(proc);
    }
    CloseHandle(foto);
    return pids;
}

namespace {
struct DadosDaJanela {
    DWORD pid;
};
BOOL CALLBACK pedirParaFechar(HWND janela, LPARAM parametro)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(janela, &pid);
    if (pid == reinterpret_cast<DadosDaJanela *>(parametro)->pid)
        PostMessageW(janela, WM_CLOSE, 0, 0);
    return TRUE;
}
}  // namespace

bool fecharProcessos(const std::vector<DWORD> &pids, DWORD esperaMs)
{
    std::vector<HANDLE> processos;
    for (DWORD pid : pids) {
        DadosDaJanela d{pid};
        EnumWindows(pedirParaFechar, reinterpret_cast<LPARAM>(&d));
        if (HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid))
            processos.push_back(h);
    }
    bool todosSairam = true;
    const DWORD inicio = GetTickCount();
    for (HANDLE h : processos) {
        const DWORD passou = GetTickCount() - inicio;
        const DWORD resta = passou >= esperaMs ? 0 : esperaMs - passou;
        if (WaitForSingleObject(h, resta) != WAIT_OBJECT_0)
            todosSairam = false;
        CloseHandle(h);
    }
    return todosSairam;
}

bool executar(const std::wstring &exe, const std::wstring &argumentos, const std::wstring &pastaDeTrabalho, bool semJanela)
{
    std::wstring linha = L"\"" + exe + L"\"" + (argumentos.empty() ? L"" : L" " + argumentos);
    STARTUPINFOW si = {};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi = {};
    const BOOL ok = CreateProcessW(nullptr, &linha[0], nullptr, nullptr, FALSE, semJanela ? CREATE_NO_WINDOW : 0, nullptr,
                                   pastaDeTrabalho.empty() ? nullptr : pastaDeTrabalho.c_str(), &si, &pi);
    if (ok) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    return ok != 0;
}

// ---------------------------------------------------------------- Interface

std::wstring escolherPasta(HWND dono, const std::wstring &titulo, const std::wstring &inicial)
{
    std::wstring escolhida;
    IFileOpenDialog *dialogo = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog, reinterpret_cast<void **>(&dialogo))) || !dialogo)
        return escolhida;
    DWORD opcoes = 0;
    dialogo->GetOptions(&opcoes);
    dialogo->SetOptions(opcoes | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dialogo->SetTitle(titulo.c_str());
    if (!inicial.empty()) {
        IShellItem *item = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(inicial.c_str(), nullptr, IID_IShellItem, reinterpret_cast<void **>(&item))) && item) {
            dialogo->SetFolder(item);
            item->Release();
        }
    }
    if (SUCCEEDED(dialogo->Show(dono))) {
        IShellItem *resultado = nullptr;
        if (SUCCEEDED(dialogo->GetResult(&resultado)) && resultado) {
            PWSTR caminho = nullptr;
            if (SUCCEEDED(resultado->GetDisplayName(SIGDN_FILESYSPATH, &caminho)) && caminho)
                escolhida = caminho;
            CoTaskMemFree(caminho);
            resultado->Release();
        }
    }
    dialogo->Release();
    return escolhida;
}

}  // namespace Sistema
