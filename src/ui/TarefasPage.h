#pragma once

#include <QWidget>

struct Repositorios;
class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;
class TarefaRepository;
class TurmaRepository;

// Tela "Tarefas": lista completa com filtros (pendentes/concluídas, por turma),
// criação, edição, exclusão e marcação como concluída.
class TarefasPage : public QWidget {
    Q_OBJECT
public:
    explicit TarefasPage(Repositorios &repos, QWidget *parent = nullptr);

    // Mostra todas as tarefas e seleciona uma (usado pela busca e pelo calendário).
    void selecionarTarefa(int tarefaId);

protected:
    void showEvent(QShowEvent *evento) override;

private:
    void recarregarTurmas();
    void recarregar(int selecionarId = 0);
    int tarefaSelecionadaId() const;
    void nova();
    void editar();
    void excluir();
    void aoAlterarItem(QTableWidgetItem *item);
    void atualizarBotoes();

    TarefaRepository &m_tarefas;
    TurmaRepository &m_turmas;

    QComboBox *m_filtroSituacao = nullptr;
    QComboBox *m_filtroTurma = nullptr;
    QTableWidget *m_tabela = nullptr;
    QPushButton *m_btnEditar = nullptr;
    QPushButton *m_btnExcluir = nullptr;
    QLabel *m_resumo = nullptr;
    bool m_carregando = false;
};
