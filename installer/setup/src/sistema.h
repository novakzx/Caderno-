#pragma once

#include <windows.h>

#include <string>
#include <vector>

// Pequenas funções do Windows usadas pelo instalador (texto, pastas, atalhos, registro, processos).
namespace Sistema {

// --- Texto ---
std::wstring paraLargo(const std::string &utf8);
std::string paraUtf8(const std::wstring &largo);

// --- Caminhos ---
std::wstring caminhoDoExecutavel();
std::wstring pastaDoArquivo(const std::wstring &caminho);
std::wstring juntar(const std::wstring &a, const std::wstring &b);
std::wstring pastaConhecida(const GUID &id);  // SHGetKnownFolderPath
std::wstring pastaTemporaria();
bool existe(const std::wstring &caminho);
bool ehPastaVazia(const std::wstring &pasta);
bool criarPastas(const std::wstring &pasta);  // cria também as pastas-mãe
// Caminho relativo seguro (sem "..", sem unidade, sem barra no início)? Barras "/" viram "\".
bool caminhoRelativoSeguro(const std::wstring &relativo);

// --- Arquivos ---
bool escreverArquivo(const std::wstring &caminho, const void *dados, size_t tamanho, std::wstring *erro);
bool lerArquivoTexto(const std::wstring &caminho, std::string *conteudo);
bool apagarArquivo(const std::wstring &caminho);
bool apagarPastaSeVazia(const std::wstring &pasta);
bool apagarArvore(const std::wstring &pasta);  // só para a pasta de dados, depois de conferida pelo chamador
unsigned long long espacoLivre(const std::wstring &pasta);  // em bytes (0 se não souber)

// --- Atalhos (.lnk) ---
bool criarAtalho(const std::wstring &caminhoDoLnk, const std::wstring &alvo, const std::wstring &pastaDeTrabalho, const std::wstring &descricao);

// --- Registro (HKCU) ---
bool gravarTexto(const std::wstring &chave, const std::wstring &nome, const std::wstring &valor);
bool gravarNumero(const std::wstring &chave, const std::wstring &nome, DWORD valor);
bool lerTexto(const std::wstring &chave, const std::wstring &nome, std::wstring *valor);
bool apagarChave(const std::wstring &chave);  // com subchaves

// --- Processos ---
// Há algum processo com este executável em execução? (caminho completo, sem diferenciar maiúsculas)
std::vector<DWORD> processosDoExecutavel(const std::wstring &caminhoDoExe);
// Pede educadamente que fechem (WM_CLOSE) e espera até `esperaMs`. Devolve true se não sobrou nenhum.
bool fecharProcessos(const std::vector<DWORD> &pids, DWORD esperaMs);
bool executar(const std::wstring &exe, const std::wstring &argumentos, const std::wstring &pastaDeTrabalho, bool semJanela);

// --- Interface ---
std::wstring escolherPasta(HWND dono, const std::wstring &titulo, const std::wstring &inicial);

}  // namespace Sistema
