#pragma once

#include "models/Horario.h"

#include <QWidget>

class HorarioGridWidget;
class HorarioRepository;
class QCheckBox;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QTimer;
class TurmaRepository;

// Tela "Horário": grade semanal com as aulas de todas as turmas. Valida
// conflitos (duas aulas no mesmo horário) e grava pelo HorarioRepository.
class HorarioPage : public QWidget {
    Q_OBJECT
public:
    HorarioPage(HorarioRepository &horarios, TurmaRepository &turmas, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *evento) override;

private:
    void recarregar();
    void atualizarLegenda(const QList<Horario> &aulas);

    void novaAula(int diaSemana, const QTime &inicio);
    void editarAula(int id);
    void excluirAula(int id);
    void alterarAula(int id, int diaSemana, const QTime &inicio, const QTime &fim);

    // Mensagem de erro se o horário conflita com outra aula; vazio se estiver livre.
    QString validarConflito(const Horario &h);
    void mostrarMensagem(const QString &texto, bool erro);

    HorarioRepository &m_horarios;
    TurmaRepository &m_turmas;

    HorarioGridWidget *m_grade = nullptr;
    QCheckBox *m_fimDeSemana = nullptr;
    QPushButton *m_btnNovaAula = nullptr;
    QHBoxLayout *m_legenda = nullptr;
    QLabel *m_dica = nullptr;
    QLabel *m_mensagem = nullptr;
    QTimer *m_timerMensagem = nullptr;
    QTimer *m_timerRelogio = nullptr;
};
