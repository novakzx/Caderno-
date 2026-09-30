#pragma once

#include "models/Horario.h"
#include "models/Turma.h"

#include <QDialog>
#include <QList>
#include <functional>

class QComboBox;
class QLineEdit;
class QTimeEdit;

// Diálogo para criar ou editar uma aula da grade semanal.
class HorarioDialog : public QDialog {
    Q_OBJECT
public:
    // Devolve uma mensagem de erro (texto não vazio) se o horário não for aceito,
    // ou texto vazio se estiver tudo certo. Usado para checar conflitos com o banco
    // sem que o diálogo conheça o repositório.
    using Validador = std::function<QString(const Horario &)>;

    HorarioDialog(const QList<Turma> &turmas, const Horario &inicial, bool edicao,
                  Validador validador, QWidget *parent = nullptr);

    Horario horario() const;

protected:
    void accept() override;

private:
    Horario m_horario;
    QList<Turma> m_turmas;
    Validador m_validador;
    QString m_salaSugerida;

    QComboBox *m_turma = nullptr;
    QComboBox *m_dia = nullptr;
    QTimeEdit *m_inicio = nullptr;
    QTimeEdit *m_fim = nullptr;
    QLineEdit *m_sala = nullptr;
};
