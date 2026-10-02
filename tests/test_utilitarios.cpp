// Teste simples (sem framework) dos utilitários puros: versões, sorteios e calendário .ics.
// Rodar: ctest --test-dir build   (ou executar o binário test_utilitarios)
#include "core/IcsUtil.h"
#include "core/SorteioUtil.h"
#include "core/VersaoUtil.h"

#include <algorithm>
#include <cstdio>
#include <set>

static int falhas = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FALHOU (linha %d): %s\n", __LINE__, #cond);       \
            ++falhas;                                                      \
        }                                                                  \
    } while (0)

int main()
{
    // --- Versões ---
    CHECK(VersaoUtil::ehMaisNova("v1.2.0", "1.1.0"));
    CHECK(VersaoUtil::ehMaisNova("1.10.0", "1.9.9"));      // 10 > 9 (comparação numérica, não de texto)
    CHECK(VersaoUtil::ehMaisNova("2.0.0", "1.99.99"));
    CHECK(!VersaoUtil::ehMaisNova("1.1.0", "1.1.0"));      // igual não é mais nova
    CHECK(!VersaoUtil::ehMaisNova("1.0.9", "1.1.0"));
    CHECK(!VersaoUtil::ehMaisNova("lixo", "1.1.0"));
    CHECK(!VersaoUtil::ehMaisNova("v1.2", "1.1.0"));       // formato incompleto
    CHECK(!VersaoUtil::ehMaisNova("v1.2.0-beta", "1.1.0"));
    CHECK(!VersaoUtil::ehMaisNova("v1.2.0.1", "1.1.0"));
    CHECK(!VersaoUtil::ehMaisNova("v9999999.0.0", "1.1.0"));  // número absurdo é recusado
    CHECK(!VersaoUtil::ehMaisNova("", ""));
    CHECK(VersaoUtil::analisar("V3.4.5").has_value());
    CHECK(!VersaoUtil::analisar("1..2").has_value());
    CHECK(!VersaoUtil::analisar("1.2.x").has_value());

    // --- Sorteio ---
    {
        std::mt19937 rng(42);
        const auto ordem = SorteioUtil::embaralhar(10, rng);
        CHECK(ordem.size() == 10);
        std::set<int> unicos(ordem.begin(), ordem.end());
        CHECK(unicos.size() == 10 && *unicos.begin() == 0 && *unicos.rbegin() == 9);  // é uma permutação
        CHECK(SorteioUtil::embaralhar(0, rng).empty());
        CHECK(SorteioUtil::embaralhar(-3, rng).empty());
    }
    {
        // Grupos: todos aparecem uma vez e os tamanhos diferem em no máximo 1.
        for (int n : {1, 2, 7, 10, 29, 35}) {
            for (int g : {1, 2, 3, 4, 6, 40}) {
                std::mt19937 rng(static_cast<unsigned>(n * 100 + g));
                const auto grupos = SorteioUtil::dividirEmGrupos(n, g, rng);
                CHECK(!grupos.empty());
                std::set<int> vistos;
                std::size_t menor = 1000, maior = 0;
                for (const auto &grupo : grupos) {
                    for (int i : grupo) {
                        CHECK(i >= 0 && i < n);
                        vistos.insert(i);
                    }
                    menor = std::min(menor, grupo.size());
                    maior = std::max(maior, grupo.size());
                }
                CHECK(static_cast<int>(vistos.size()) == n);
                CHECK(maior - menor <= 1);
                CHECK(static_cast<int>(grupos.size()) == std::min(g, n));
            }
        }
        std::mt19937 rng(1);
        CHECK(SorteioUtil::dividirEmGrupos(0, 3, rng).empty());
        CHECK(SorteioUtil::dividirEmGrupos(5, 0, rng).size() == 1);   // quantidade inválida vira 1 grupo
        CHECK(SorteioUtil::dividirEmGrupos(5, -2, rng).size() == 1);
        CHECK(SorteioUtil::gruposParaTamanho(10, 3) == 4);
        CHECK(SorteioUtil::gruposParaTamanho(9, 3) == 3);
        CHECK(SorteioUtil::gruposParaTamanho(0, 3) == 0);
        CHECK(SorteioUtil::gruposParaTamanho(5, 0) == 5);
    }
    {
        // Mesma semente, mesmo resultado (reprodutível nos testes).
        std::mt19937 a(7), b(7);
        CHECK(SorteioUtil::embaralhar(20, a) == SorteioUtil::embaralhar(20, b));
    }
    {
        // Sem repetição: em n sorteios, ninguém sai duas vezes; depois recomeça.
        std::mt19937 rng(99);
        SorteioUtil::SorteioSemRepeticao s(5);
        CHECK(s.restantes() == 5);  // antes do primeiro sorteio, todos faltam
        std::set<int> saiu;
        bool recomecou = false;
        for (int i = 0; i < 5; ++i) {
            const int p = s.proximo(rng, &recomecou);
            CHECK(p >= 0 && p < 5);
            CHECK(!recomecou);
            CHECK(saiu.insert(p).second);
            CHECK(s.restantes() == 4 - i);
        }
        CHECK(s.restantes() == 0);
        s.proximo(rng, &recomecou);
        CHECK(recomecou);  // a fila foi refeita
        SorteioUtil::SorteioSemRepeticao vazio(0);
        CHECK(vazio.proximo(rng) == -1);
    }

    // --- iCalendar ---
    CHECK(IcsUtil::escapar("a,b;c\\d") == "a\\,b\\;c\\\\d");
    CHECK(IcsUtil::escapar("linha1\r\nlinha2\nlinha3\rlinha4") == "linha1 linha2 linha3 linha4");
    {
        // Injeção: quebra de linha no título não pode criar um evento ou campo novo.
        const std::string mal = IcsUtil::escapar("Prova\r\nEND:VEVENT\r\nBEGIN:VEVENT\r\nSUMMARY:falso");
        CHECK(mal.find('\n') == std::string::npos && mal.find('\r') == std::string::npos);
    }
    {
        // Linha longa é dobrada em pedaços de até 75 bytes, e a união (sem o espaço de continuação) restaura o texto.
        const std::string longa = "SUMMARY:" + std::string(200, 'a');
        const std::string dobrada = IcsUtil::dobrarLinha(longa);
        std::string reconstruida;
        std::size_t pos = 0;
        bool ok = true;
        while (pos < dobrada.size()) {
            const std::size_t fim = dobrada.find("\r\n", pos);
            if (fim == std::string::npos) { ok = false; break; }
            const std::string parte = dobrada.substr(pos, fim - pos);
            if (parte.size() > 75) ok = false;
            reconstruida += (pos == 0) ? parte : parte.substr(1);
            pos = fim + 2;
        }
        CHECK(ok && reconstruida == longa);
    }
    {
        // Não corta um caractere UTF-8 ao meio ("ã" = 2 bytes).
        const std::string acentuada = "SUMMARY:" + std::string(66, 'a') + "\xC3\xA3\xC3\xA3\xC3\xA3\xC3\xA3";
        const std::string dobrada = IcsUtil::dobrarLinha(acentuada);
        bool boaFronteira = true;
        std::size_t pos = 0;
        while (pos < dobrada.size()) {
            const std::size_t fim = dobrada.find("\r\n", pos);
            const std::string parte = dobrada.substr(pos, fim - pos);
            const std::size_t inicio = (pos == 0) ? 0 : 1;
            if (parte.size() > inicio && (static_cast<unsigned char>(parte[inicio]) & 0xC0) == 0x80)
                boaFronteira = false;  // começou no meio de um caractere
            pos = fim + 2;
        }
        CHECK(boaFronteira);
    }
    {
        int a = 2026, m = 12, d = 31;
        IcsUtil::diaSeguinte(a, m, d);
        CHECK(a == 2027 && m == 1 && d == 1);
        a = 2028; m = 2; d = 28;
        IcsUtil::diaSeguinte(a, m, d);
        CHECK(a == 2028 && m == 2 && d == 29);  // 2028 é bissexto
        a = 2026; m = 2; d = 28;
        IcsUtil::diaSeguinte(a, m, d);
        CHECK(a == 2026 && m == 3 && d == 1);
    }
    {
        IcsUtil::Evento e1;
        e1.uid = "evento-1@caderno";
        e1.titulo = "Prova de Matemática, 8º A";
        e1.descricao = "Capítulos 1;2";
        e1.ano = 2026; e1.mes = 10; e1.dia = 6;
        IcsUtil::Evento e2;  // várias datas: 12 a 13/10 (inclusivo) -> DTEND 14/10
        e2.uid = "evento-2@caderno";
        e2.titulo = "Recesso";
        e2.ano = 2026; e2.mes = 10; e2.dia = 12;
        e2.anoFim = 2026; e2.mesFim = 10; e2.diaFim = 13;
        const std::string ics = IcsUtil::montar({e1, e2}, "Caderno+", "20261002T120000Z");
        CHECK(ics.rfind("BEGIN:VCALENDAR\r\n", 0) == 0);
        CHECK(ics.find("END:VCALENDAR\r\n") == ics.size() - std::string("END:VCALENDAR\r\n").size());
        CHECK(ics.find("DTSTART;VALUE=DATE:20261006\r\n") != std::string::npos);
        CHECK(ics.find("DTEND;VALUE=DATE:20261007\r\n") != std::string::npos);   // exclusivo: dia seguinte
        CHECK(ics.find("DTEND;VALUE=DATE:20261014\r\n") != std::string::npos);
        CHECK(ics.find("SUMMARY:Prova de Matem") != std::string::npos);
        CHECK(ics.find("\\, 8") != std::string::npos);                            // vírgula escapada
        CHECK(ics.find("Cap") != std::string::npos && ics.find("1\\;2") != std::string::npos);
        // Todo fim de linha é CRLF (nenhum \n solto).
        bool soCrlf = true;
        for (std::size_t i = 0; i < ics.size(); ++i)
            if (ics[i] == '\n' && (i == 0 || ics[i - 1] != '\r'))
                soCrlf = false;
        CHECK(soCrlf);
        // Dois BEGIN:VEVENT exatamente (nenhum a mais, nem por injeção).
        std::size_t conta = 0, pos = 0;
        while ((pos = ics.find("BEGIN:VEVENT", pos)) != std::string::npos) { ++conta; pos += 5; }
        CHECK(conta == 2);
    }

    if (falhas == 0)
        std::printf("OK: versoes, sorteios e calendario ics.\n");
    return falhas == 0 ? 0 : 1;
}
