// Teste simples (sem framework) das regras do painel "Alunos em atenção" e dos tipos de ocorrência.
// Rodar: ctest --test-dir build   (ou executar o binário test_atencao)
#include "core/AtencaoUtil.h"
#include "core/OcorrenciaUtil.h"

#include <cstdio>

static int falhas = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FALHOU (linha %d): %s\n", __LINE__, #cond);       \
            ++falhas;                                                      \
        }                                                                  \
    } while (0)

using namespace AtencaoUtil;

static bool tem(const Resultado &r, Motivo m)
{
    for (Motivo x : r.motivos)
        if (x == m)
            return true;
    return false;
}

int main()
{
    // --- Sem informação, sem alerta ---
    {
        const Resultado r = avaliar(Entrada{});
        CHECK(r.nivel == Nivel::Nenhum);
        CHECK(r.motivos.empty());
    }

    // --- Média: abaixo = crítico; no limite = ok; perto = atenção ---
    {
        Entrada e;
        e.media = 5.9;
        const Resultado r = avaliar(e);
        CHECK(r.nivel == Nivel::Critico);
        CHECK(tem(r, Motivo::MediaAbaixo));
    }
    {
        Entrada e;
        e.media = 6.0;  // exatamente na nota de corte: aprovado, mas "perto"
        const Resultado r = avaliar(e);
        CHECK(r.nivel == Nivel::Atencao);
        CHECK(tem(r, Motivo::MediaPerto));
        CHECK(!tem(r, Motivo::MediaAbaixo));
    }
    {
        Entrada e;
        e.media = 6.5;  // corte + margem: já fora da faixa de atenção
        CHECK(avaliar(e).nivel == Nivel::Nenhum);
        e.media = 9.0;
        CHECK(avaliar(e).nivel == Nivel::Nenhum);
    }
    {
        Limites l;
        l.notaCorte = 7.0;  // a nota de corte da escola é configurável
        Entrada e;
        e.media = 6.5;
        CHECK(avaliar(e, l).nivel == Nivel::Critico);
    }

    // --- Frequência ---
    {
        Entrada e;
        e.frequenciaPct = 74.9;
        const Resultado r = avaliar(e);
        CHECK(r.nivel == Nivel::Critico);
        CHECK(tem(r, Motivo::FrequenciaAbaixo));
    }
    {
        Entrada e;
        e.frequenciaPct = 75.0;  // no mínimo: ainda aprovado, mas perto
        const Resultado r = avaliar(e);
        CHECK(r.nivel == Nivel::Atencao);
        CHECK(tem(r, Motivo::FrequenciaPerto));
    }
    {
        Entrada e;
        e.frequenciaPct = 80.0;
        CHECK(avaliar(e).nivel == Nivel::Nenhum);
        e.frequenciaPct = 100.0;
        CHECK(avaliar(e).nivel == Nivel::Nenhum);
    }

    // --- Ocorrências negativas recentes ---
    {
        Entrada e;
        e.ocorrenciasNegativasRecentes = 1;
        CHECK(avaliar(e).nivel == Nivel::Nenhum);
        e.ocorrenciasNegativasRecentes = 2;
        const Resultado r = avaliar(e);
        CHECK(r.nivel == Nivel::Atencao);
        CHECK(tem(r, Motivo::Ocorrencias));
    }
    {
        Limites l;
        l.ocorrenciasNegativas = 0;  // 0 desliga o critério
        Entrada e;
        e.ocorrenciasNegativasRecentes = 50;
        CHECK(avaliar(e, l).nivel == Nivel::Nenhum);
    }

    // --- Vários motivos: o mais grave define o nível; todos aparecem, na ordem ---
    {
        Entrada e;
        e.media = 4.0;
        e.frequenciaPct = 78.0;
        e.ocorrenciasNegativasRecentes = 3;
        const Resultado r = avaliar(e);
        CHECK(r.nivel == Nivel::Critico);
        CHECK(r.motivos.size() == 3);
        CHECK(r.motivos[0] == Motivo::MediaAbaixo);
        CHECK(r.motivos[1] == Motivo::FrequenciaPerto);
        CHECK(r.motivos[2] == Motivo::Ocorrencias);
    }

    // --- Rótulos existem para todos os motivos ---
    for (Motivo m : {Motivo::MediaAbaixo, Motivo::MediaPerto, Motivo::FrequenciaAbaixo, Motivo::FrequenciaPerto,
                     Motivo::Ocorrencias})
        CHECK(rotulo(m)[0] != '\0');

    // --- Tipos de ocorrência ---
    CHECK(OcorrenciaUtil::idValido("elogio"));
    CHECK(OcorrenciaUtil::idValido("outro"));
    CHECK(!OcorrenciaUtil::idValido("inexistente"));
    CHECK(!OcorrenciaUtil::idValido(""));
    CHECK(OcorrenciaUtil::ehNegativa("conduta"));
    CHECK(OcorrenciaUtil::ehNegativa("dificuldade"));
    CHECK(!OcorrenciaUtil::ehNegativa("elogio"));
    CHECK(!OcorrenciaUtil::ehNegativa("familia"));
    CHECK(!OcorrenciaUtil::ehNegativa("inexistente"));
    CHECK(std::string_view(OcorrenciaUtil::tipoDe("inexistente").id) == "outro");  // id desconhecido cai em "outro"
    CHECK(std::string_view(OcorrenciaUtil::tipoDe("elogio").rotulo) == "Elogio");
    for (const auto &t : OcorrenciaUtil::kTipos)
        CHECK(OcorrenciaUtil::idValido(t.id));

    if (falhas == 0)
        std::printf("OK: alunos em atencao e ocorrencias.\n");
    return falhas == 0 ? 0 : 1;
}
