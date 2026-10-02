// Teste simples (sem framework) do leitor de CSV usado na importação de alunos.
// Rodar: ctest --test-dir build   (ou executar o binário test_csv)
#include "core/CsvUtil.h"

#include <cstdio>

static int falhas = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FALHOU (linha %d): %s\n", __LINE__, #cond);       \
            ++falhas;                                                      \
        }                                                                  \
    } while (0)

using namespace CsvUtil;

int main()
{
    // --- Separador ---
    CHECK(detectarSeparador("Nome;Matricula\nAna;1") == ';');
    CHECK(detectarSeparador("Nome,Matricula\nAna,1") == ',');
    CHECK(detectarSeparador("Nome\tMatricula\nAna\t1") == '\t');
    CHECK(detectarSeparador("Ana Souza\nBruno Lima") == ';');           // lista de nomes: tanto faz
    CHECK(detectarSeparador("") == ';');
    CHECK(detectarSeparador("\n\nNome,Matricula") == ',');              // linhas vazias antes do cabeçalho
    CHECK(detectarSeparador("\"Souza, Ana\";1") == ';');                // a vírgula entre aspas não conta
    CHECK(detectarSeparador("a;b;c,d") == ';');

    // --- Leitura simples ---
    {
        const Tabela t = analisar("Nome;Mat\nAna;1\nBruno;2\n", ';');
        CHECK(t.size() == 3);
        CHECK(t[0].size() == 2 && t[0][0] == "Nome" && t[0][1] == "Mat");
        CHECK(t[2][0] == "Bruno" && t[2][1] == "2");
    }
    // sem quebra de linha no fim
    {
        const Tabela t = analisar("a;b\nc;d", ';');
        CHECK(t.size() == 2 && t[1][1] == "d");
    }
    // CRLF, CR e linhas vazias
    {
        const Tabela t = analisar("a;b\r\n\r\nc;d\r\n", ';');
        CHECK(t.size() == 2);
        const Tabela t2 = analisar("a;b\rc;d\r", ';');
        CHECK(t2.size() == 2 && t2[1][0] == "c");
    }
    // campos vazios e linha só de separadores (mantida: quem usa decide)
    {
        const Tabela t = analisar("a;;c\n;;\n", ';');
        CHECK(t.size() == 2);
        CHECK(t[0].size() == 3 && t[0][1].empty() && t[0][2] == "c");
        CHECK(t[1].size() == 3);
    }

    // --- Aspas ---
    {
        const Tabela t = analisar("\"Souza, Ana\";\"1\"\n", ';');
        CHECK(t.size() == 1 && t[0][0] == "Souza, Ana" && t[0][1] == "1");
    }
    {
        const Tabela t = analisar("\"Ela disse \"\"oi\"\"\";x\n", ';');
        CHECK(t.size() == 1 && t[0][0] == "Ela disse \"oi\"" && t[0][1] == "x");
    }
    {
        const Tabela t = analisar("\"linha1\nlinha2\";x\nfim;y\n", ';');  // quebra de linha dentro de aspas
        CHECK(t.size() == 2 && t[0][0] == "linha1\nlinha2" && t[1][0] == "fim");
    }
    {
        const Tabela t = analisar("\"sem fechar;x\ny;z\n", ';');  // aspas sem fechar: engole até o fim, sem travar
        CHECK(t.size() == 1);
    }

    // --- Textos que parecem fórmulas continuam sendo só texto ---
    {
        const Tabela t = analisar("=HYPERLINK(\"http://x\");@SUM(1)\n", ';');
        CHECK(t.size() == 1 && t[0][1] == "@SUM(1)");
    }

    // --- UTF-8 passa intacto ---
    {
        const Tabela t = analisar("Jo\xC3\xA3o;Concei\xC3\xA7\xC3\xA3o\n", ';');
        CHECK(t.size() == 1 && t[0][0] == "Jo\xC3\xA3o" && t[0][1] == "Concei\xC3\xA7\xC3\xA3o");
    }

    // --- Tabulação (colar do Excel) ---
    {
        const Tabela t = analisar("Nome\tMat\nAna Souza\t7\n", '\t');
        CHECK(t.size() == 2 && t[1][0] == "Ana Souza" && t[1][1] == "7");
    }

    if (falhas == 0)
        std::printf("OK: leitor de CSV.\n");
    return falhas == 0 ? 0 : 1;
}
