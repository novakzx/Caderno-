#pragma once

#include "models/Turma.h"

#include <QDialog>

class QButtonGroup;
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
    // Seletor de cor: as seis cores do design (turma-1..turma-6) como sugestões
    // e "Outra cor…" para uma cor livre.
    QWidget *criarSeletorDeCor();
    void escolherCorLivre();
    void atualizarSeletorDeCor();

    Turma m_turma;
    QLineEdit *m_nome = nullptr;
    QLineEdit *m_disciplina = nullptr;
    QSpinBox *m_ano = nullptr;
    QLineEdit *m_periodo = nullptr;
    QLineEdit *m_sala = nullptr;
    QButtonGroup *m_grupoCor = nullptr;   // ids 1..6 = turma-N; 7 = cor livre
    QPushButton *m_botaoCorLivre = nullptr;
    QCheckBox *m_arquivada = nullptr;
};
