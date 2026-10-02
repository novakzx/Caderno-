#pragma once

#include <optional>
#include <vector>

// Regras do painel "Alunos em atenção", sem Qt (testável: tests/test_atencao.cpp).
//
// Um aluno entra no painel por um ou mais motivos:
//  - média abaixo da nota de corte (crítico) ou até `margemMedia` acima dela (atenção);
//  - frequência abaixo do mínimo de 75% (crítico) ou até `margemFrequencia` acima dele (atenção);
//  - várias ocorrências negativas (conduta, dificuldade) recentes (atenção).
// Dado que falta (sem nota ou sem chamada registrada) nunca vira motivo: sem informação, sem alerta.
namespace AtencaoUtil {

enum class Motivo { MediaAbaixo, MediaPerto, FrequenciaAbaixo, FrequenciaPerto, Ocorrencias };
enum class Nivel { Nenhum, Atencao, Critico };

struct Limites {
    double notaCorte = 6.0;             // média mínima (a mesma "nota de corte" da tela de Notas)
    double margemMedia = 0.5;           // "perto" = corte <= média < corte + margem
    double frequenciaMinima = 75.0;     // LDB (igual a FrequenciaUtil::kFrequenciaMinima)
    double margemFrequencia = 5.0;
    int ocorrenciasNegativas = 2;       // a partir de quantas ocorrências negativas recentes alertar
    int diasDeOcorrencias = 30;         // "recentes" = últimos N dias
};

struct Entrada {
    std::optional<double> media;          // 0-10
    std::optional<double> frequenciaPct;  // 0-100
    int ocorrenciasNegativasRecentes = 0;
};

struct Resultado {
    Nivel nivel = Nivel::Nenhum;
    std::vector<Motivo> motivos;  // na ordem: média, frequência, ocorrências
};

inline Resultado avaliar(const Entrada &e, const Limites &l = Limites())
{
    Resultado r;
    bool critico = false;

    if (e.media) {
        if (*e.media < l.notaCorte) {
            r.motivos.push_back(Motivo::MediaAbaixo);
            critico = true;
        } else if (*e.media < l.notaCorte + l.margemMedia) {
            r.motivos.push_back(Motivo::MediaPerto);
        }
    }
    if (e.frequenciaPct) {
        if (*e.frequenciaPct < l.frequenciaMinima) {
            r.motivos.push_back(Motivo::FrequenciaAbaixo);
            critico = true;
        } else if (*e.frequenciaPct < l.frequenciaMinima + l.margemFrequencia) {
            r.motivos.push_back(Motivo::FrequenciaPerto);
        }
    }
    if (l.ocorrenciasNegativas > 0 && e.ocorrenciasNegativasRecentes >= l.ocorrenciasNegativas)
        r.motivos.push_back(Motivo::Ocorrencias);

    if (r.motivos.empty())
        r.nivel = Nivel::Nenhum;
    else
        r.nivel = critico ? Nivel::Critico : Nivel::Atencao;
    return r;
}

// Texto curto de cada motivo (UTF-8), para a lista e os relatórios.
inline const char *rotulo(Motivo m)
{
    switch (m) {
    case Motivo::MediaAbaixo: return "Média abaixo da nota de corte";
    case Motivo::MediaPerto: return "Média perto da nota de corte";
    case Motivo::FrequenciaAbaixo: return "Frequência abaixo de 75%";
    case Motivo::FrequenciaPerto: return "Frequência perto do mínimo";
    case Motivo::Ocorrencias: return "Ocorrências recentes";
    }
    return "";
}

}  // namespace AtencaoUtil
