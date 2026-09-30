#pragma once

#include "models/Tarefa.h"
#include "models/Turma.h"

#include <QDialog>
#include <QList>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLineEdit;
class QPlainTextEdit;

// Diálogo para criar ou editar uma tarefa.
class TarefaDialog : public QDialog {
    Q_OBJECT
public:
    TarefaDialog(const QList<Turma> &turmas, const Tarefa &inicial, bool edicao, QWidget *parent = nullptr);

    Tarefa tarefa() const;

protected:
    void accept() override;

private:
    Tarefa m_tarefa;
    QLineEdit *m_titulo = nullptr;
    QPlainTextEdit *m_descricao = nullptr;
    QComboBox *m_turma = nullptr;
    QCheckBox *m_temPrazo = nullptr;
    QDateEdit *m_prazo = nullptr;
    QComboBox *m_prioridade = nullptr;
    QCheckBox *m_concluida = nullptr;
};
