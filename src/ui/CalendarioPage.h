#pragma once

#include <QDate>
#include <QList>
#include <QMap>
#include <QWidget>

class AgendaRepository;
class EventoRepository;
struct Repositorios;
class QCalendarWidget;
class QLabel;
class QListWidget;
class QPushButton;
class TarefaRepository;
class TurmaRepository;

// Tela "Calendário": calendário do mês com os eventos escolares (provas,
// feriados, reuniões), os prazos das tarefas e as avaliações com data.
// O dia selecionado mostra tudo o que acontece nele.
class CalendarioPage : public QWidget {
    Q_OBJECT
public:
    explicit CalendarioPage(Repositorios &repos, QWidget *parent = nullptr);

    void irParaData(const QDate &data);

signals:
    // Duplo clique num prazo de tarefa: a janela principal abre a tela de tarefas.
    void abrirTarefaSolicitada(int tarefaId);

protected:
    void showEvent(QShowEvent *evento) override;

private:
    // Um item que aparece num dia do calendário.
    struct ItemDia {
        QString categoria;  // evento, prova, feriado, recesso, reuniao, tarefa, avaliacao
        QString texto;
        int id = 0;         // id do evento ou da tarefa (0 para avaliações)
    };

    void carregarPeriodoVisivel();
    void mostrarDiaSelecionado();
    void atualizarBotoes();
    void novoEvento();
    void editarEvento();
    void excluirEvento();
    void abrirItemSelecionado();
    const ItemDia *itemSelecionado() const;

    EventoRepository &m_eventos;
    TarefaRepository &m_tarefas;
    TurmaRepository &m_turmas;
    AgendaRepository &m_agenda;

    QMap<QDate, QList<ItemDia>> m_itens;  // itens do período visível, por dia
    QList<ItemDia> m_itensDoDia;          // itens do dia selecionado (mesma ordem da lista)

    QCalendarWidget *m_calendario = nullptr;
    QLabel *m_tituloDia = nullptr;
    QListWidget *m_lista = nullptr;
    QPushButton *m_btnNovo = nullptr;
    QPushButton *m_btnEditar = nullptr;
    QPushButton *m_btnExcluir = nullptr;
};
