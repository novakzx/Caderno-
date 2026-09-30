#pragma once

#include "models/Avaliacao.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;

// Diálogo para criar ou editar uma avaliação (coluna da planilha de notas).
class AvaliacaoDialog : public QDialog {
    Q_OBJECT
public:
    // `existente` = nullptr cria nova na turma `turmaId`, já no período `periodoPadrao`.
    AvaliacaoDialog(int turmaId, int periodoPadrao, const Avaliacao *existente = nullptr,
                    QWidget *parent = nullptr);

    Avaliacao avaliacao() const;

protected:
    void accept() override;

private:
    Avaliacao m_avaliacao;
    QLineEdit *m_nome = nullptr;
    QComboBox *m_tipo = nullptr;
    QDoubleSpinBox *m_peso = nullptr;
    QDoubleSpinBox *m_notaMaxima = nullptr;
    QSpinBox *m_periodo = nullptr;
    QCheckBox *m_temData = nullptr;
    QDateEdit *m_data = nullptr;
};
