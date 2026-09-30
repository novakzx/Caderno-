#include "ui/HorarioDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QTimeEdit>
#include <QVBoxLayout>

HorarioDialog::HorarioDialog(const QList<Turma> &turmas, const Horario &inicial, bool edicao,
                             Validador validador, QWidget *parent)
    : QDialog(parent), m_horario(inicial), m_turmas(turmas), m_validador(std::move(validador))
{
    setWindowTitle(edicao ? QStringLiteral("Editar aula") : QStringLiteral("Nova aula"));
    setMinimumWidth(400);

    // Turma (com quadradinho da cor)
    m_turma = new QComboBox;
    for (const Turma &t : m_turmas) {
        QPixmap pm(14, 14);
        pm.fill(QColor(t.cor));
        QString texto = t.nome;
        if (!t.disciplina.isEmpty())
            texto += QStringLiteral(" — ") + t.disciplina;
        m_turma->addItem(QIcon(pm), texto, t.id);
    }
    const int idxTurma = m_turma->findData(inicial.turmaId);
    m_turma->setCurrentIndex(idxTurma >= 0 ? idxTurma : 0);

    m_dia = new QComboBox;
    const char *dias[] = {"Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira",
                          "Sexta-feira", "Sábado", "Domingo"};
    for (int i = 0; i < 7; ++i)
        m_dia->addItem(QString::fromUtf8(dias[i]), i + 1);
    m_dia->setCurrentIndex(qBound(0, inicial.diaSemana - 1, 6));

    m_inicio = new QTimeEdit(inicial.inicio.isValid() ? inicial.inicio : QTime(7, 30));
    m_fim = new QTimeEdit(inicial.fim.isValid() ? inicial.fim : QTime(8, 20));
    m_inicio->setDisplayFormat(QStringLiteral("HH:mm"));
    m_fim->setDisplayFormat(QStringLiteral("HH:mm"));

    // A sala vem da turma por padrão, mas pode ser trocada para esta aula.
    m_sala = new QLineEdit(inicial.sala);
    auto sugerirSala = [this] {
        const int i = m_turma->currentIndex();
        const QString nova = (i >= 0 && i < m_turmas.size()) ? m_turmas.at(i).sala : QString();
        if (m_sala->text().isEmpty() || m_sala->text() == m_salaSugerida)
            m_sala->setText(nova);
        m_salaSugerida = nova;
    };
    m_salaSugerida = QString();
    if (inicial.sala.isEmpty())
        sugerirSala();
    else
        m_salaSugerida = inicial.sala;
    connect(m_turma, &QComboBox::currentIndexChanged, this, sugerirSala);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Turma *"), m_turma);
    form->addRow(QStringLiteral("Dia"), m_dia);
    form->addRow(QStringLiteral("Início"), m_inicio);
    form->addRow(QStringLiteral("Fim"), m_fim);
    form->addRow(QStringLiteral("Sala"), m_sala);

    auto *botoes = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    botoes->button(QDialogButtonBox::Save)->setText(QStringLiteral("Salvar"));
    botoes->button(QDialogButtonBox::Save)->setObjectName(QStringLiteral("primary"));
    botoes->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
    connect(botoes, &QDialogButtonBox::accepted, this, &HorarioDialog::accept);
    connect(botoes, &QDialogButtonBox::rejected, this, &HorarioDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(14);
    layout->addLayout(form);
    layout->addWidget(botoes);
}

Horario HorarioDialog::horario() const
{
    Horario h = m_horario;  // mantém o id
    h.turmaId = m_turma->currentData().toInt();
    h.diaSemana = m_dia->currentData().toInt();
    h.inicio = m_inicio->time();
    h.fim = m_fim->time();
    h.sala = m_sala->text().trimmed();
    return h;
}

void HorarioDialog::accept()
{
    const Horario h = horario();
    if (h.turmaId <= 0) {
        QMessageBox::warning(this, QStringLiteral("Campo obrigatório"), QStringLiteral("Escolha a turma."));
        return;
    }
    if (h.fim <= h.inicio) {
        QMessageBox::warning(this, QStringLiteral("Horário inválido"),
                             QStringLiteral("O horário de término precisa ser depois do início."));
        m_fim->setFocus();
        return;
    }
    if (m_validador) {
        const QString erro = m_validador(h);
        if (!erro.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Horário indisponível"), erro);
            return;
        }
    }
    QDialog::accept();
}
