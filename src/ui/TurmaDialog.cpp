#include "ui/TurmaDialog.h"

#include "ui/ThemeManager.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QDate>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
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

    m_arquivada = new QCheckBox(QStringLiteral("Turma arquivada (oculta da lista)"));
    m_arquivada->setChecked(m_turma.arquivada);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Nome *"), m_nome);
    form->addRow(QStringLiteral("Disciplina"), m_disciplina);
    form->addRow(QStringLiteral("Ano letivo"), m_ano);
    form->addRow(QStringLiteral("Período"), m_periodo);
    form->addRow(QStringLiteral("Sala"), m_sala);
    form->addRow(QStringLiteral("Cor"), criarSeletorDeCor());
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

namespace {

constexpr int kIdCorLivre = 7;  // ids 1..6 = turma-1..turma-6

// Nome de cada cor sugerida (o nome identifica; a cor só reforça).
QString nomeDaCorSugerida(int numero)
{
    static const char *nomes[] = {"Azul", "Ocre", "Ameixa", "Terracota", "Oliva", "Lousa"};
    return QString::fromUtf8(nomes[qBound(1, numero, 6) - 1]);
}

// Estilo de um botão colorido: fundo da cor, texto legível por cima, borda de destaque se marcado.
QString estiloDoBotaoDeCor(const QString &corSalva)
{
    return QStringLiteral("QPushButton { background: %1; color: %2; font-weight: 600; border: 2px solid transparent; }"
                          "QPushButton:hover { background: %1; }"
                          "QPushButton:checked { border: 2px solid %3; }")
        .arg(ThemeManager::corDaTurma(corSalva).name(), ThemeManager::textoSobreTurma(corSalva).name(),
             ThemeManager::corHex(Tokens::Id::Ink));
}

}  // namespace

QWidget *TurmaDialog::criarSeletorDeCor()
{
    auto *container = new QWidget;
    auto *grade = new QGridLayout(container);
    grade->setContentsMargins(0, 0, 0, 0);
    grade->setSpacing(8);

    m_grupoCor = new QButtonGroup(this);
    m_grupoCor->setExclusive(true);

    // As seis cores do design como sugestões (turma-1..turma-6).
    for (int n = 1; n <= Tokens::kTotalTurmas; ++n) {
        auto *botao = new QPushButton(nomeDaCorSugerida(n));
        botao->setCheckable(true);
        botao->setCursor(Qt::PointingHandCursor);
        botao->setStyleSheet(estiloDoBotaoDeCor(QString::fromLatin1(Tokens::nomeDaTurma(n))));
        m_grupoCor->addButton(botao, n);
        grade->addWidget(botao, (n - 1) / 3, (n - 1) % 3);
    }

    // Cor livre, para quem quiser uma cor fora das sugestões.
    m_botaoCorLivre = new QPushButton(QStringLiteral("Outra cor…"));
    m_botaoCorLivre->setCheckable(true);
    m_botaoCorLivre->setCursor(Qt::PointingHandCursor);
    m_grupoCor->addButton(m_botaoCorLivre, kIdCorLivre);
    grade->addWidget(m_botaoCorLivre, 2, 0, 1, 3);

    connect(m_grupoCor, &QButtonGroup::idClicked, this, [this](int id) {
        if (id == kIdCorLivre)
            escolherCorLivre();
        else
            m_turma.cor = QString::fromLatin1(Tokens::nomeDaTurma(id));
        atualizarSeletorDeCor();
    });

    atualizarSeletorDeCor();
    return container;
}

void TurmaDialog::escolherCorLivre()
{
    const QColor escolhida = QColorDialog::getColor(ThemeManager::corDaTurma(m_turma.cor), this,
                                                    QStringLiteral("Cor da turma"));
    if (escolhida.isValid())
        m_turma.cor = escolhida.name();  // "#rrggbb"
}

// Marca o botão da cor atual (com um ✓ em ícone, além da borda) e pinta o "Outra cor…" se for cor livre.
void TurmaDialog::atualizarSeletorDeCor()
{
    const int numero = Tokens::numeroDaTurma(m_turma.cor.toUtf8().constData());
    const int idAtual = numero > 0 ? numero : kIdCorLivre;

    for (QAbstractButton *botao : m_grupoCor->buttons()) {
        const bool marcado = m_grupoCor->id(botao) == idAtual;
        botao->setChecked(marcado);
        const QString corDoBotao = botao == m_botaoCorLivre
                                       ? m_turma.cor
                                       : QString::fromLatin1(Tokens::nomeDaTurma(m_grupoCor->id(botao)));
        botao->setIcon(marcado ? ThemeManager::iconeColorido(QStringLiteral("check"),
                                                              ThemeManager::textoSobreTurma(corDoBotao), 16)
                               : QIcon());
        if (botao == m_botaoCorLivre) {
            botao->setStyleSheet(numero == 0 ? estiloDoBotaoDeCor(corDoBotao) : QString());
            botao->setText(numero == 0 ? QStringLiteral("Outra cor (%1)…").arg(m_turma.cor.toUpper())
                                       : QStringLiteral("Outra cor…"));
        }
    }
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
