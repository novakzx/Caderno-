#pragma once

#include "core/LembreteUtil.h"

#include <QDateTime>
#include <QList>
#include <QString>

class AgendaRepository;
class TarefaRepository;

// Um aviso a mostrar. `chave` identifica o aviso (para não repetir o mesmo no mesmo dia).
struct Lembrete {
    QString chave;
    QString titulo;
    QString texto;
};

// Descobre quais lembretes valem AGORA (aulas prestes a começar, tarefas e provas do dia e do dia
// seguinte). Só consulta os repositórios e aplica as regras de core/LembreteUtil.h: não mostra nada
// (quem mostra é ui/GerenteDeLembretes) e não guarda o que já foi avisado.
class LembreteService {
public:
    LembreteService(AgendaRepository &agenda, TarefaRepository &tarefas) : m_agenda(agenda), m_tarefas(tarefas) {}

    QList<Lembrete> pendentes(const QDateTime &agora, const LembreteUtil::Config &config);

private:
    AgendaRepository &m_agenda;
    TarefaRepository &m_tarefas;
};
