#include "ui/EventoDialog.h"
#include "ui/ThemeManager.h"

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

EventoDialog::EventoDialog(const QList<Turma> &turmas, const Evento &inicial, bool edicao, QWidget *parent)
    : QDialog(parent), m_evento(inicial)
{
    setWindowTitle(edicao ? QStringLiteral("Editar evento") : QStringLiteral("Novo evento"));
    setMinimumWidth(440);

    m_titulo = new QLineEdit(inicial.titulo);
    m_titulo->setPlaceholderText(QStringLiteral("Ex.: Prova de Matemática"));

    // Valores gravados no banco e textos mostrados ao usuário.
    m_tipo = new QComboBox;
    auto icone = [](const char *nome) {
        return ThemeManager::iconeColorido(QLatin1String(nome), Tokens::Id::InkMuted, 16);
    };
    m_tipo->addItem(icone("anotacoes"), QStringLiteral("Prova"), QStringLiteral("prova"));
    m_tipo->addItem(icone("feriado"), QStringLiteral("Feriado"), QStringLiteral("feriado"));
    m_tipo->addItem(icone("recesso"), QStringLiteral("Recesso escolar"), QStringLiteral("recesso"));
    m_tipo->addItem(icone("turmas"), QStringLiteral("Reunião"), QStringLiteral("reuniao"));
    m_tipo->addItem(icone("pin"), QStringLiteral("Outro evento"), QStringLiteral("evento"));
    const int idxTipo = m_tipo->findData(inicial.tipo);
    m_tipo->setCurrentIndex(idxTipo >= 0 ? idxTipo : 4);

    m_turma = new QComboBox;
    m_turma->addItem(QStringLiteral("Geral (todas as turmas)"), 0);
    for (const Turma &t : turmas)
        m_turma->addItem(t.nome, t.id);
    const int idxTurma = m_turma->findData(inicial.turmaId);
    m_turma->setCurrentIndex(idxTurma >= 0 ? idxTurma : 0);

    m_inicio = new QDateEdit(inicial.dataInicio.isValid() ? inicial.dataInicio : QDate::currentDate());
    m_inicio->setCalendarPopup(true);
    m_inicio->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));

    m_variosDias = new QCheckBox(QStringLiteral("Dura vários dias"));
    m_fim = new QDateEdit(inicial.dataFim.isValid() ? inicial.dataFim : m_inicio->date());
    m_fim->setCalendarPopup(true);
    m_fim->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    m_variosDias->setChecked(inicial.dataFim.isValid() && inicial.dataFim != inicial.dataInicio);
    m_fim->setEnabled(m_variosDias->isChecked());
    connect(m_variosDias, &QCheckBox::toggled, m_fim, &QWidget::setEnabled);
    // Ao mudar o início, o fim acompanha se ainda ficaria antes dele.
    connect(m_inicio, &QDateEdit::dateChanged, this, [this](const QDate &d) {
        if (m_fim->date() < d)
            m_fim->setDate(d);
    });
    auto *linhaFim = new QHBoxLayout;
    linhaFim->addWidget(m_fim, 1);
    linhaFim->addWidget(m_variosDias);

    m_descricao = new QPlainTextEdit(inicial.descricao);
    m_descricao->setFixedHeight(70);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Título *"), m_titulo);
    form->addRow(QStringLiteral("Tipo"), m_tipo);
    form->addRow(QStringLiteral("Turma"), m_turma);
    form->addRow(QStringLiteral("Início"), m_inicio);
    form->addRow(QStringLiteral("Fim"), linhaFim);
    form->addRow(QStringLiteral("Detalhes"), m_descricao);

    auto *botoes = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    botoes->button(QDialogButtonBox::Save)->setText(QStringLiteral("Salvar"));
    botoes->button(QDialogButtonBox::Save)->setObjectName(QStringLiteral("primary"));
    botoes->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
    connect(botoes, &QDialogButtonBox::accepted, this, &EventoDialog::accept);
    connect(botoes, &QDialogButtonBox::rejected, this, &EventoDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(14);
    layout->addLayout(form);
    layout->addWidget(botoes);
}

Evento EventoDialog::evento() const
{
    Evento e = m_evento;  // mantém o id
    e.titulo = m_titulo->text().trimmed();
    e.tipo = m_tipo->currentData().toString();
    e.turmaId = m_turma->currentData().toInt();
    e.dataInicio = m_inicio->date();
    e.dataFim = (m_variosDias->isChecked() && m_fim->date() > m_inicio->date()) ? m_fim->date() : QDate();
    e.descricao = m_descricao->toPlainText().trimmed();
    return e;
}

void EventoDialog::accept()
{
    if (m_titulo->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Campo obrigatório"), QStringLiteral("Informe o título do evento."));
        m_titulo->setFocus();
        return;
    }
    if (m_variosDias->isChecked() && m_fim->date() < m_inicio->date()) {
        QMessageBox::warning(this, QStringLiteral("Datas inválidas"),
                             QStringLiteral("A data final não pode ser anterior à inicial."));
        return;
    }
    QDialog::accept();
}
