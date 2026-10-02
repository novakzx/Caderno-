#pragma once

#include <string>
#include <vector>

// Leitura de CSV (texto separado por vírgula, ponto e vírgula ou tabulação), sem Qt
// (testável: tests/test_csv.cpp). Trabalha com texto UTF-8; quem chama decodifica o arquivo antes.
//
// Regras (as do formato CSV "de verdade", que o Excel e o Google Planilhas geram):
//  - campo entre aspas pode ter o separador e quebras de linha dentro; "" dentro de aspas vira uma aspa;
//  - a linha termina em \n, \r\n ou \r; linhas totalmente vazias são ignoradas;
//  - nenhum campo é "interpretado": tudo é texto (quem usa o valor é que valida).
namespace CsvUtil {

using Linha = std::vector<std::string>;
using Tabela = std::vector<Linha>;

// Escolhe o separador pela primeira linha com conteúdo (ignorando o que está entre aspas).
// Empate ou nenhuma ocorrência: ';' (padrão do Excel em português).
inline char detectarSeparador(const std::string &texto)
{
    int pontoEVirgula = 0, virgula = 0, tabulacao = 0;
    bool entreAspas = false, viuConteudo = false;
    for (const char c : texto) {
        if (c == '"') {
            entreAspas = !entreAspas;
            viuConteudo = true;
        } else if (entreAspas) {
            continue;
        } else if (c == '\n' || c == '\r') {
            if (viuConteudo)
                break;
        } else {
            viuConteudo = true;
            if (c == ';') ++pontoEVirgula;
            else if (c == ',') ++virgula;
            else if (c == '\t') ++tabulacao;
        }
    }
    if (pontoEVirgula == 0 && virgula == 0 && tabulacao == 0)
        return ';';
    if (pontoEVirgula >= virgula && pontoEVirgula >= tabulacao)
        return ';';
    if (tabulacao >= virgula)
        return '\t';
    return ',';
}

inline Tabela analisar(const std::string &texto, char separador)
{
    Tabela tabela;
    Linha linha;
    std::string campo;
    bool entreAspas = false;

    auto fecharCampo = [&] {
        linha.push_back(campo);
        campo.clear();
    };
    auto fecharLinha = [&] {
        fecharCampo();
        const bool vazia = linha.size() == 1 && linha.front().empty();
        if (!vazia)
            tabela.push_back(linha);
        linha.clear();
    };

    for (std::size_t i = 0; i < texto.size(); ++i) {
        const char c = texto[i];
        if (entreAspas) {
            if (c == '"') {
                if (i + 1 < texto.size() && texto[i + 1] == '"') {
                    campo += '"';
                    ++i;
                } else {
                    entreAspas = false;
                }
            } else {
                campo += c;
            }
        } else if (c == '"') {
            entreAspas = true;
        } else if (c == separador) {
            fecharCampo();
        } else if (c == '\r') {
            if (i + 1 < texto.size() && texto[i + 1] == '\n')
                ++i;
            fecharLinha();
        } else if (c == '\n') {
            fecharLinha();
        } else {
            campo += c;
        }
    }
    if (!campo.empty() || !linha.empty())
        fecharLinha();
    return tabela;
}

}  // namespace CsvUtil
