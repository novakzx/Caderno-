#pragma once

#include <QWidget>

class AgendaRepository;
class QLabel;
class QLineEdit;
class QTimer;
class QVBoxLayout;
class TarefaRepository;

// Painel "Hoje": aulas do dia (com destaque para a aula em andamento),
// tarefas pendentes (com campo para criar e marcar como feita) e provas
// dos próximos dias. Atualiza sozinho a cada minuto e ao abrir a tela.
class HojePage : public QWidget {
    Q_OBJECT
public:
    HojePage(AgendaRepository &agenda, TarefaRepository &tarefas, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *evento) override;

private:
    void atualizar();
    void atualizarAulas(const QDate &hoje, const QTime &agora, int *totalAulas);
    void atualizarTarefas(const QDate &hoje, int *totalTarefas);
    void atualizarProvas(const QDate &hoje, int *totalProvas);
    void criarTarefaRapida();

    AgendaRepository &m_agenda;
    TarefaRepository &m_tarefas;

    QLabel *m_data = nullptr;
    QLabel *m_resumo = nullptr;
    QVBoxLayout *m_listaAulas = nullptr;
    QVBoxLayout *m_listaTarefas = nullptr;
    QVBoxLayout *m_listaProvas = nullptr;
    QLineEdit *m_novaTarefa = nullptr;
    QTimer *m_timer = nullptr;
};
