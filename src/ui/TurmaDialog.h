#pragma once

#include "models/Turma.h"

#include <QDialog>

class QLineEdit;
class QPushButton;
class QSpinBox;
class QCheckBox;

// Diálogo para criar ou editar uma turma. Só coleta e valida os dados;
// quem grava no banco é a tela que o chamou (via TurmaRepository).
class TurmaDialog : public QDialog {
    Q_OBJECT
public:
    // `existente` = nullptr cria uma turma nova; caso contrário edita uma cópia.
    explicit TurmaDialog(const Turma *existente = nullptr, QWidget *parent = nullptr);

    // Turma com os valores preenchidos (mantém o id da turma editada).
    Turma turma() const;

protected:
    void accept() override;

private:
    void escolherCor();
    void atualizarBotaoCor();

    Turma m_turma;
    QLineEdit *m_nome = nullptr;
    QLineEdit *m_disciplina = nullptr;
    QSpinBox *m_ano = nullptr;
    QLineEdit *m_periodo = nullptr;
    QLineEdit *m_sala = nullptr;
    QPushButton *m_botaoCor = nullptr;
    QCheckBox *m_arquivada = nullptr;
};
