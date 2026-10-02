#pragma once

#include <QDialog>

class GerenteDeLembretes;
class QCheckBox;
class QComboBox;

// Preferências dos lembretes do Windows: liga/desliga, antecedência das aulas, avisos de tarefas e provas,
// e um botão para mandar uma notificação de teste.
class LembretesDialog : public QDialog {
    Q_OBJECT
public:
    LembretesDialog(GerenteDeLembretes &gerente, QWidget *parent = nullptr);

private:
    void salvar();

    GerenteDeLembretes &m_gerente;
    QCheckBox *m_ativos = nullptr;
    QComboBox *m_antecedencia = nullptr;
    QCheckBox *m_prazos = nullptr;
};
