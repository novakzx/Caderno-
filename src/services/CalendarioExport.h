#pragma once

#include <QByteArray>
#include <QDate>
#include <QDateTime>

class AgendaRepository;
class EventoRepository;
class TarefaRepository;

// Gera o arquivo .ics (iCalendar) com o calendário escolar: eventos (provas, feriados, reuniões...),
// prazos das tarefas ainda pendentes e avaliações com data. Abre no Google Agenda, Outlook e no celular.
namespace CalendarioExport {

// `de`..`ate` limita o período. `total` (opcional) recebe quantos itens entraram.
QByteArray gerarIcs(EventoRepository &eventos, TarefaRepository &tarefas, AgendaRepository &agenda, const QDate &de,
                    const QDate &ate, const QDateTime &geradoEm, int *total = nullptr);

}  // namespace CalendarioExport
