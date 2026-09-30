#include "ui/TarefaDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

TarefaDialog::TarefaDialog(const QList<Turma> &turmas, const Tarefa &inicial, bool edicao, QWidget *parent)
    : QDialog(parent), m_tarefa(inicial)
{
    setWindowTitle(edicao ? QStringLiteral("Editar tarefa") : QStringLiteral("Nova tarefa"));
    setMinimumWidth(440);

    m_titulo = new QLineEdit(inicial.titulo);
    m_titulo->setPlaceholderText(QStringLiteral("Ex.: Corrigir provas do 8º A"));
    m_descricao = new QPlainTextEdit(inicial.descricao);
    m_descricao->setFixedHeight(80);

    m_turma = new QComboBox;
    m_turma->addItem(QStringLiteral("Sem turma"), 0);
    for (const Turma &t : turmas)
        m_turma->addItem(t.nome, t.id);
    const int idxTurma = m_turma->findData(inicial.turmaId);
    m_turma->setCurrentIndex(idxTurma >= 0 ? idxTurma : 0);

    // Prazo opcional
    m_temPrazo = new QCheckBox(QStringLiteral("Definir prazo"));
    m_prazo = new QDateEdit;
    m_prazo->setCalendarPopup(true);
    m_prazo->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    m_prazo->setDate(inicial.dataEntrega.isValid() ? inicial.dataEntrega : QDate::currentDate());
    m_temPrazo->setChecked(inicial.dataEntrega.isValid());
    m_prazo->setEnabled(m_temPrazo->isChecked());
    connect(m_temPrazo, &QCheckBox::toggled, m_prazo, &QWidget::setEnabled);
    auto *linhaPrazo = new QHBoxLayout;
    linhaPrazo->addWidget(m_prazo, 1);
    linhaPrazo->addWidget(m_temPrazo);

    m_prioridade = new QComboBox;
    m_prioridade->addItem(QStringLiteral("Baixa"), 0);
    m_prioridade->addItem(QStringLiteral("Normal"), 1);
    m_prioridade->addItem(QStringLiteral("Alta"), 2);
    m_prioridade->setCurrentIndex(qBound(0, inicial.prioridade, 2));

    m_concluida = new QCheckBox(QStringLiteral("Tarefa concluída"));
    m_concluida->setChecked(inicial.concluida);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Título *"), m_titulo);
    form->addRow(QStringLiteral("Descrição"), m_descricao);
    form->addRow(QStringLiteral("Turma"), m_turma);
    form->addRow(QStringLiteral("Prazo"), linhaPrazo);
    form->addRow(QStringLiteral("Prioridade"), m_prioridade);
    form->addRow(QString(), m_concluida);

    auto *botoes = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    botoes->button(QDialogButtonBox::Save)->setText(QStringLiteral("Salvar"));
    botoes->button(QDialogButtonBox::Save)->setObjectName(QStringLiteral("primary"));
    botoes->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
    connect(botoes, &QDialogButtonBox::accepted, this, &TarefaDialog::accept);
    connect(botoes, &QDialogButtonBox::rejected, this, &TarefaDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(14);
    layout->addLayout(form);
    layout->addWidget(botoes);
}

Tarefa TarefaDialog::tarefa() const
{
    Tarefa t = m_tarefa;  // mantém o id
    t.titulo = m_titulo->text().trimmed();
    t.descricao = m_descricao->toPlainText().trimmed();
    t.turmaId = m_turma->currentData().toInt();
    t.dataEntrega = m_temPrazo->isChecked() ? m_prazo->date() : QDate();
    t.prioridade = m_prioridade->currentData().toInt();
    t.concluida = m_concluida->isChecked();
    return t;
}

void TarefaDialog::accept()
{
    if (m_titulo->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Campo obrigatório"), QStringLiteral("Informe o título da tarefa."));
        m_titulo->setFocus();
        return;
    }
    QDialog::accept();
}
