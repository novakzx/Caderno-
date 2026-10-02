#pragma once

#include "services/DesempenhoService.h"

#include <QWidget>

class AgendaRepository;
class QLabel;
class QLineEdit;
class QTimer;
class QVBoxLayout;
class TarefaRepository;
struct Repositorios;

// Painel "Hoje": aulas do dia (com destaque para a aula em andamento),
// tarefas pendentes (com campo para criar e marcar como feita), alunos em atenção
// (média, frequência e ocorrências) e provas dos próximos dias.
// Atualiza sozinho a cada minuto e ao abrir a tela.
class HojePage : public QWidget {
    Q_OBJECT
public:
    explicit HojePage(Repositorios &repos, QWidget *parent = nullptr);

signals:
    // Clique no nome de um aluno do cartão "Alunos em atenção": a janela abre a turma com o aluno selecionado.
    void abrirAlunoSolicitado(int turmaId, int alunoId);

protected:
    void showEvent(QShowEvent *evento) override;

private:
    void atualizar();
    void atualizarAulas(const QDate &hoje, const QTime &agora, int *totalAulas);
    void atualizarTarefas(const QDate &hoje, int *totalTarefas);
    void atualizarProvas(const QDate &hoje, int *totalProvas);
    void atualizarAtencao(int *totalAlunos);
    void criarTarefaRapida();

    AgendaRepository &m_agenda;
    TarefaRepository &m_tarefas;
    DesempenhoService m_desempenho;

    QLabel *m_data = nullptr;
    QLabel *m_resumo = nullptr;
    QLabel *m_numAulas = nullptr;
    QLabel *m_numTarefas = nullptr;
    QLabel *m_numProvas = nullptr;
    QLabel *m_numAtencao = nullptr;
    QVBoxLayout *m_listaAulas = nullptr;
    QVBoxLayout *m_listaTarefas = nullptr;
    QVBoxLayout *m_listaProvas = nullptr;
    QVBoxLayout *m_listaAtencao = nullptr;
    QLineEdit *m_novaTarefa = nullptr;
    QTimer *m_timer = nullptr;
};
