#pragma once

#include "models/Aluno.h"

#include <QDialog>

class QCheckBox;
class QDateEdit;
class QLineEdit;
class QPlainTextEdit;

// Diálogo para criar ou editar um aluno (só coleta e valida os dados).
class AlunoDialog : public QDialog {
    Q_OBJECT
public:
    // `existente` = nullptr cria um aluno novo na turma `turmaId`.
    AlunoDialog(int turmaId, const Aluno *existente = nullptr, QWidget *parent = nullptr);

    Aluno aluno() const;

protected:
    void accept() override;

private:
    Aluno m_aluno;
    QLineEdit *m_nome = nullptr;
    QLineEdit *m_matricula = nullptr;
    QLineEdit *m_email = nullptr;
    QCheckBox *m_temNascimento = nullptr;
    QDateEdit *m_nascimento = nullptr;
    QPlainTextEdit *m_observacoes = nullptr;
    QCheckBox *m_ativo = nullptr;
};
