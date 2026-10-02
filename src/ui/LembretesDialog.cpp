#include "ui/LembretesDialog.h"

#include "ui/GerenteDeLembretes.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QString rotuloDaAntecedencia(int minutos)
{
    if (minutos == 0)
        return QStringLiteral("Não avisar das aulas");
    return QStringLiteral("%1 minutos antes").arg(minutos);
}

}  // namespace

LembretesDialog::LembretesDialog(GerenteDeLembretes &gerente, QWidget *parent) : QDialog(parent), m_gerente(gerente)
{
    setWindowTitle(QStringLiteral("Lembretes"));
    setMinimumWidth(480);

    const LembreteUtil::Config atual = GerenteDeLembretes::configuracao();

    auto *titulo = new QLabel(QStringLiteral("Lembretes do Windows"));
    titulo->setObjectName(QStringLiteral("sectionTitle"));
    auto *explicacao = new QLabel(QStringLiteral("O Caderno+ avisa com uma notificação do Windows, ao lado do relógio. "
                                                 "Os avisos aparecem enquanto o programa estiver aberto."));
    explicacao->setObjectName(QStringLiteral("muted"));
    explicacao->setWordWrap(true);

    m_ativos = new QCheckBox(QStringLiteral("Ativar lembretes"));
    m_ativos->setChecked(atual.ativos);

    m_antecedencia = new QComboBox;
    for (int minutos : LembreteUtil::kAntecedenciasPossiveis)
        m_antecedencia->addItem(rotuloDaAntecedencia(minutos), minutos);
    m_antecedencia->setCurrentIndex(qMax(0, m_antecedencia->findData(atual.antecedenciaAula)));

    m_prazos = new QCheckBox(QStringLiteral("Avisar de tarefas e provas de hoje e de amanhã (a partir das 8h)"));
    m_prazos->setChecked(atual.prazos);

    auto *form = new QFormLayout;
    form->addRow(QString(), m_ativos);
    form->addRow(QStringLiteral("Aulas"), m_antecedencia);
    form->addRow(QStringLiteral("Prazos"), m_prazos);

    auto *teste = new QPushButton(QStringLiteral("Enviar notificação de teste"));
    auto *dica = new QLabel(QStringLiteral("Se a notificação de teste não aparecer, o \"Assistente de foco\" do Windows pode "
                                           "estar ligado (Configurações > Sistema > Foco)."));
    dica->setObjectName(QStringLiteral("muted"));
    dica->setWordWrap(true);

    auto *indisponivel = new QLabel(QStringLiteral("Este computador não oferece a área de notificação do Windows; "
                                                   "os lembretes não podem ser mostrados."));
    indisponivel->setWordWrap(true);
    indisponivel->setVisible(!gerente.bandejaDisponivel());

    auto *botoes = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    botoes->button(QDialogButtonBox::Save)->setText(QStringLiteral("Salvar"));
    botoes->button(QDialogButtonBox::Save)->setObjectName(QStringLiteral("primary"));
    botoes->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
    connect(botoes, &QDialogButtonBox::accepted, this, &LembretesDialog::salvar);
    connect(botoes, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(teste, &QPushButton::clicked, this, [this] { m_gerente.notificarTeste(); });

    teste->setEnabled(gerente.bandejaDisponivel());
    m_ativos->setEnabled(gerente.bandejaDisponivel());

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(12);
    layout->addWidget(titulo);
    layout->addWidget(explicacao);
    layout->addLayout(form);
    layout->addWidget(indisponivel);
    layout->addWidget(teste, 0, Qt::AlignLeft);
    layout->addWidget(dica);
    layout->addStretch(1);
    layout->addWidget(botoes);
}

void LembretesDialog::salvar()
{
    LembreteUtil::Config c;
    c.ativos = m_ativos->isChecked();
    c.antecedenciaAula = m_antecedencia->currentData().toInt();
    c.prazos = m_prazos->isChecked();
    GerenteDeLembretes::salvarConfiguracao(c);
    m_gerente.configuracaoAlterada();
    accept();
}
