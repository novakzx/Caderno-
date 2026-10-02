#pragma once

// Regras dos lembretes do Windows, sem Qt (testável: tests/test_lembrete.cpp).
//
//  - Aula: avisa nos últimos `antecedenciaAula` minutos antes de começar (nunca depois que começou).
//  - Tarefa e prova: avisam no dia anterior e no próprio dia, a partir das 8h (não acordam ninguém de madrugada).
//    Tarefa atrasada não entra: ela já aparece em vermelho no painel "Hoje".
namespace LembreteUtil {

struct Config {
    bool ativos = true;
    int antecedenciaAula = 10;  // minutos; 0 = não avisar das aulas
    bool prazos = true;         // avisar tarefas e provas do dia e do dia seguinte
};

constexpr int kHoraDosPrazos = 8;
constexpr int kAntecedenciasPossiveis[] = {0, 5, 10, 15, 30};

inline bool antecedenciaValida(int minutos)
{
    for (int m : kAntecedenciasPossiveis)
        if (m == minutos)
            return true;
    return false;
}

// Minutos que faltam para a aula, ou -1 se não é hora de avisar.
// `minutoAgora` e `minutoInicio` são minutos desde a meia-noite.
inline int minutosParaAvisarAula(int minutoAgora, int minutoInicio, int antecedencia)
{
    if (antecedencia <= 0)
        return -1;
    const int faltam = minutoInicio - minutoAgora;
    return (faltam > 0 && faltam <= antecedencia) ? faltam : -1;
}

// `diasAteOPrazo`: 0 = hoje, 1 = amanhã, negativo = atrasado.
inline bool deveAvisarPrazo(int diasAteOPrazo, int horaAgora)
{
    return diasAteOPrazo >= 0 && diasAteOPrazo <= 1 && horaAgora >= kHoraDosPrazos;
}

}  // namespace LembreteUtil
