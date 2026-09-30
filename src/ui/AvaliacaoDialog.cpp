#include "ui/AvaliacaoDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
// (valor gravado no banco, texto mostrado)
const QList<QPair<QString, QString>> kTipos = {
    {"prova", "Prova"},
    {"trabalho", "Trabalho"},
    {"atividade", "Atividade"},
    {"participacao", "Participação"},
    {"outro", "Outro"},
};
}  // namespace

AvaliacaoDialog::AvaliacaoDialog(int turmaId, int periodoPadrao, const Avaliacao *existente,
                                 QWidget *parent)
    : QDialog(parent)
{
    if (existente) {
        m_avaliacao = *existente;
    } else {
        m_avaliacao.turmaId = turmaId;
        m_avaliacao.periodo = qBound(1, periodoPadrao, 4);
    }

    setWindowTitle(existente ? QStringLiteral("Editar avaliação") : QStringLiteral("Nova avaliação"));
    setMinimumWidth(420);

    m_nome = new QLineEdit(m_avaliacao.nome);
    m_nome->setPlaceholderText(QStringLiteral("Ex.: Prova 1"));

    m_tipo = new QComboBox;
    for (const auto &t : kTipos)
        m_tipo->addItem(t.second, t.first);
    const int idxTipo = m_tipo->findData(m_avaliacao.tipo);
    m_tipo->setCurrentIndex(idxTipo >= 0 ? idxTipo : 0);

    m_peso = new QDoubleSpinBox;
    m_peso->setRange(0.01, 100.0);
    m_peso->setDecimals(2);
    m_peso->setValue(m_avaliacao.peso);

    m_notaMaxima = new QDoubleSpinBox;
    m_notaMaxima->setRange(0.1, 1000.0);
    m_notaMaxima->setDecimals(2);
    m_notaMaxima->setValue(m_avaliacao.notaMaxima);

    m_periodo = new QSpinBox;
    m_periodo->setRange(1, 4);
    m_periodo->setSuffix(QStringLiteral("º período (bimestre/trimestre)"));
    m_periodo->setValue(m_avaliacao.periodo);

    m_temData = new QCheckBox(QStringLiteral("Informar"));
    m_data = new QDateEdit;
    m_data->setCalendarPopup(true);
    m_data->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    m_data->setDate(m_avaliacao.data.isValid() ? m_avaliacao.data : QDate::currentDate());
    m_temData->setChecked(m_avaliacao.data.isValid());
    m_data->setEnabled(m_temData->isChecked());
    connect(m_temData, &QCheckBox::toggled, m_data, &QWidget::setEnabled);

    auto *linhaData = new QHBoxLayout;
    linhaData->addWidget(m_data, 1);
    linhaData->addWidget(m_temData);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Nome *"), m_nome);
    form->addRow(QStringLiteral("Tipo"), m_tipo);
    form->addRow(QStringLiteral("Peso"), m_peso);
    form->addRow(QStringLiteral("Nota máxima"), m_notaMaxima);
    form->addRow(QStringLiteral("Período"), m_periodo);
    form->addRow(QStringLiteral("Data"), linhaData);

    auto *botoes = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    botoes->button(QDialogButtonBox::Save)->setText(QStringLiteral("Salvar"));
    botoes->button(QDialogButtonBox::Save)->setObjectName(QStringLiteral("primary"));
    botoes->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
    connect(botoes, &QDialogButtonBox::accepted, this, &AvaliacaoDialog::accept);
    connect(botoes, &QDialogButtonBox::rejected, this, &AvaliacaoDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(14);
    layout->addLayout(form);
    layout->addWidget(botoes);
}

Avaliacao AvaliacaoDialog::avaliacao() const
{
    Avaliacao a = m_avaliacao;  // mantém id, turmaId e ordem
    a.nome = m_nome->text().trimmed();
    a.tipo = m_tipo->currentData().toString();
    a.peso = m_peso->value();
    a.notaMaxima = m_notaMaxima->value();
    a.periodo = m_periodo->value();
    a.data = m_temData->isChecked() ? m_data->date() : QDate();
    return a;
}

void AvaliacaoDialog::accept()
{
    if (m_nome->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Campo obrigatório"),
                             QStringLiteral("Informe o nome da avaliação."));
        m_nome->setFocus();
        return;
    }
    QDialog::accept();
}
