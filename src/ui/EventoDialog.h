#pragma once

#include "models/Evento.h"
#include "models/Turma.h"

#include <QDialog>
#include <QList>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLineEdit;
class QPlainTextEdit;

// Diálogo para criar ou editar um evento do calendário escolar.
class EventoDialog : public QDialog {
    Q_OBJECT
public:
    EventoDialog(const QList<Turma> &turmas, const Evento &inicial, bool edicao, QWidget *parent = nullptr);

    Evento evento() const;

protected:
    void accept() override;

private:
    Evento m_evento;
    QLineEdit *m_titulo = nullptr;
    QComboBox *m_tipo = nullptr;
    QComboBox *m_turma = nullptr;
    QDateEdit *m_inicio = nullptr;
    QCheckBox *m_variosDias = nullptr;
    QDateEdit *m_fim = nullptr;
    QPlainTextEdit *m_descricao = nullptr;
};
