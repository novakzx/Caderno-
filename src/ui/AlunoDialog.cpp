#include "ui/AlunoDialog.h"

#include <QCheckBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

AlunoDialog::AlunoDialog(int turmaId, const Aluno *existente, QWidget *parent)
    : QDialog(parent)
{
    if (existente)
        m_aluno = *existente;
    else
        m_aluno.turmaId = turmaId;

    setWindowTitle(existente ? QStringLiteral("Editar aluno") : QStringLiteral("Novo aluno"));
    setMinimumWidth(460);

    m_nome = new QLineEdit(m_aluno.nome);
    m_matricula = new QLineEdit(m_aluno.matricula);
    m_email = new QLineEdit(m_aluno.email);

    // Data de nascimento é opcional: o checkbox liga/desliga o campo.
    m_temNascimento = new QCheckBox(QStringLiteral("Informar"));
    m_nascimento = new QDateEdit;
    m_nascimento->setCalendarPopup(true);
    m_nascimento->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    m_nascimento->setDate(m_aluno.dataNascimento.isValid() ? m_aluno.dataNascimento
                                                           : QDate(2012, 1, 1));
    m_temNascimento->setChecked(m_aluno.dataNascimento.isValid());
    m_nascimento->setEnabled(m_temNascimento->isChecked());
    connect(m_temNascimento, &QCheckBox::toggled, m_nascimento, &QWidget::setEnabled);

    auto *linhaNascimento = new QHBoxLayout;
    linhaNascimento->addWidget(m_nascimento, 1);
    linhaNascimento->addWidget(m_temNascimento);

    m_observacoes = new QPlainTextEdit(m_aluno.observacoes);
    m_observacoes->setFixedHeight(80);

    m_ativo = new QCheckBox(QStringLiteral("Aluno ativo (desmarque se foi transferido ou desistiu)"));
    m_ativo->setChecked(m_aluno.ativo);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Nome *"), m_nome);
    form->addRow(QStringLiteral("Matrícula"), m_matricula);
    form->addRow(QStringLiteral("E-mail"), m_email);
    form->addRow(QStringLiteral("Nascimento"), linhaNascimento);
    form->addRow(QStringLiteral("Observações"), m_observacoes);
    form->addRow(QString(), m_ativo);

    auto *botoes = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    botoes->button(QDialogButtonBox::Save)->setText(QStringLiteral("Salvar"));
    botoes->button(QDialogButtonBox::Save)->setObjectName(QStringLiteral("primary"));
    botoes->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
    connect(botoes, &QDialogButtonBox::accepted, this, &AlunoDialog::accept);
    connect(botoes, &QDialogButtonBox::rejected, this, &AlunoDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(14);
    layout->addLayout(form);
    layout->addWidget(botoes);
}

Aluno AlunoDialog::aluno() const
{
    Aluno a = m_aluno;  // mantém id e turmaId
    a.nome = m_nome->text().trimmed();
    a.matricula = m_matricula->text().trimmed();
    a.email = m_email->text().trimmed();
    a.dataNascimento = m_temNascimento->isChecked() ? m_nascimento->date() : QDate();
    a.observacoes = m_observacoes->toPlainText().trimmed();
    a.ativo = m_ativo->isChecked();
    return a;
}

void AlunoDialog::accept()
{
    if (m_nome->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Campo obrigatório"),
                             QStringLiteral("Informe o nome do aluno."));
        m_nome->setFocus();
        return;
    }

    // Validação simples de e-mail (opcional, mas se vier preenchido precisa ter cara de e-mail).
    const QString email = m_email->text().trimmed();
    static const QRegularExpression padraoEmail(QStringLiteral(R"(^[^@\s]+@[^@\s]+\.[^@\s]+$)"));
    if (!email.isEmpty() && !padraoEmail.match(email).hasMatch()) {
        QMessageBox::warning(this, QStringLiteral("E-mail inválido"),
                             QStringLiteral("Confira o endereço de e-mail informado."));
        m_email->setFocus();
        return;
    }
    QDialog::accept();
}
