#pragma once

#include <QDate>
#include <QWidget>

class AlunoRepository;
class EventoRepository;
class FrequenciaRepository;
struct Repositorios;
class QComboBox;
class QDateEdit;
class QLabel;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableWidget;
class TurmaRepository;

// Tela "Frequência": chamada do dia (presente, falta, justificada, atraso) e
// resumo do mês em forma de grade (alunos x dias).
class FrequenciaPage : public QWidget {
    Q_OBJECT
public:
    explicit FrequenciaPage(Repositorios &repos, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *evento) override;

private:
    void recarregarTurmas();
    void recarregarChamada();
    void recarregarResumo();
    void atualizarResumoDoDia();
    void mudarDia(int dias);
    int turmaAtualId() const;

    TurmaRepository &m_turmas;
    AlunoRepository &m_alunos;
    FrequenciaRepository &m_frequencia;
    EventoRepository &m_eventos;

    QComboBox *m_comboTurma = nullptr;
    QTabWidget *m_abas = nullptr;

    // Aba "Chamada do dia"
    QDateEdit *m_data = nullptr;
    QLabel *m_aviso = nullptr;
    QLabel *m_resumoDia = nullptr;
    QTableWidget *m_tabelaChamada = nullptr;
    QPushButton *m_btnTodosPresentes = nullptr;

    // Aba "Resumo do mês"
    QComboBox *m_mes = nullptr;
    QSpinBox *m_ano = nullptr;
    QTableWidget *m_tabelaMes = nullptr;
    QLabel *m_legendaMes = nullptr;
};
