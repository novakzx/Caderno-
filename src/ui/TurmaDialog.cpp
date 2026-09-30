#include "ui/TurmaDialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDate>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

TurmaDialog::TurmaDialog(const Turma *existente, QWidget *parent)
    : QDialog(parent)
{
    if (existente)
        m_turma = *existente;
    else
        m_turma.anoLetivo = QDate::currentDate().year();

    setWindowTitle(existente ? QStringLiteral("Editar turma") : QStringLiteral("Nova turma"));
    setMinimumWidth(420);

    m_nome = new QLineEdit(m_turma.nome);
    m_nome->setPlaceholderText(QStringLiteral("Ex.: 8º A"));
    m_disciplina = new QLineEdit(m_turma.disciplina);
    m_disciplina->setPlaceholderText(QStringLiteral("Ex.: Matemática"));

    m_ano = new QSpinBox;
    m_ano->setRange(2000, 2100);
    m_ano->setValue(m_turma.anoLetivo);

    m_periodo = new QLineEdit(m_turma.periodo);
    m_periodo->setPlaceholderText(QStringLiteral("Ex.: Manhã"));
    m_sala = new QLineEdit(m_turma.sala);

    m_botaoCor = new QPushButton;
    m_botaoCor->setCursor(Qt::PointingHandCursor);
    connect(m_botaoCor, &QPushButton::clicked, this, &TurmaDialog::escolherCor);
    atualizarBotaoCor();

    m_arquivada = new QCheckBox(QStringLiteral("Turma arquivada (oculta da lista)"));
    m_arquivada->setChecked(m_turma.arquivada);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Nome *"), m_nome);
    form->addRow(QStringLiteral("Disciplina"), m_disciplina);
    form->addRow(QStringLiteral("Ano letivo"), m_ano);
    form->addRow(QStringLiteral("Período"), m_periodo);
    form->addRow(QStringLiteral("Sala"), m_sala);
    form->addRow(QStringLiteral("Cor"), m_botaoCor);
    form->addRow(QString(), m_arquivada);

    auto *botoes = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    botoes->button(QDialogButtonBox::Save)->setText(QStringLiteral("Salvar"));
    botoes->button(QDialogButtonBox::Save)->setObjectName(QStringLiteral("primary"));
    botoes->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
    connect(botoes, &QDialogButtonBox::accepted, this, &TurmaDialog::accept);
    connect(botoes, &QDialogButtonBox::rejected, this, &TurmaDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(14);
    layout->addLayout(form);
    layout->addWidget(botoes);
}

void TurmaDialog::escolherCor()
{
    const QColor escolhida = QColorDialog::getColor(QColor(m_turma.cor), this,
                                                    QStringLiteral("Cor da turma"));
    if (escolhida.isValid()) {
        m_turma.cor = escolhida.name();  // "#rrggbb"
        atualizarBotaoCor();
    }
}

void TurmaDialog::atualizarBotaoCor()
{
    m_botaoCor->setText(m_turma.cor.toUpper());
    // Mostra a cor escolhida no próprio botão.
    m_botaoCor->setStyleSheet(QStringLiteral("QPushButton { background: %1; color: white; font-weight: 600; }")
                                  .arg(m_turma.cor));
}

Turma TurmaDialog::turma() const
{
    Turma t = m_turma;  // mantém id e cor
    t.nome = m_nome->text().trimmed();
    t.disciplina = m_disciplina->text().trimmed();
    t.anoLetivo = m_ano->value();
    t.periodo = m_periodo->text().trimmed();
    t.sala = m_sala->text().trimmed();
    t.arquivada = m_arquivada->isChecked();
    return t;
}

void TurmaDialog::accept()
{
    // Validação: o nome é obrigatório.
    if (m_nome->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Campo obrigatório"),
                             QStringLiteral("Informe o nome da turma."));
        m_nome->setFocus();
        return;
    }
    QDialog::accept();
}
