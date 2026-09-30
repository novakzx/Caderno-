#include "ui/ImportarNotasDialog.h"
#include "ui/ThemeManager.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QString numero(double v)
{
    return QString::number(v, 'g', 6).replace(QLatin1Char('.'), QLatin1Char(','));
}

}  // namespace

ImportarNotasDialog::ImportarNotasDialog(const QList<Avaliacao> &avaliacoesDaTurma,
                                         const XlsxService::Planilha &planilha,
                                         const PlanoImportacao &plano, QWidget *parent)
    : QDialog(parent), m_plano(plano)
{
    setWindowTitle(QStringLiteral("Importar notas do Excel"));
    setMinimumSize(720, 480);

    // --- Resumo dos alunos ---
    const int total = planilha.linhas.size();
    QString resumo = QStringLiteral("<b>%1</b> de %2 linha(s) da planilha correspondem a alunos da turma.")
                         .arg(plano.alunosEncontrados)
                         .arg(total);
    if (!plano.alunosNaoEncontrados.isEmpty()) {
        QStringList amostra = plano.alunosNaoEncontrados.mid(0, 8);
        resumo += QStringLiteral("<br>Não encontrados (serão ignorados): %1%2")
                      .arg(amostra.join(QStringLiteral(", ")),
                           plano.alunosNaoEncontrados.size() > 8 ? QStringLiteral(" …") : QString());
    }
    resumo += QStringLiteral("<br><span style='color:%1'>Alunos não são criados pela importação, e células vazias "
                             "não apagam notas existentes.</span>")
                  .arg(ThemeManager::corHex(Tokens::Id::InkMuted));
    if (!planilha.avisos.isEmpty())
        resumo += QStringLiteral("<br><span style='color:%2'>%1 valor(es) não numérico(s) serão ignorados.</span>")
                      .arg(planilha.avisos.size())
                      .arg(ThemeManager::corHex(Tokens::Id::Danger));

    auto *lblResumo = new QLabel(resumo);
    lblResumo->setWordWrap(true);
    lblResumo->setTextFormat(Qt::RichText);

    // --- Tabela de colunas ---
    m_tabela = new QTableWidget(m_plano.colunas.size(), 3);
    m_tabela->setHorizontalHeaderLabels({QStringLiteral("Importar"), QStringLiteral("Coluna da planilha"),
                                         QStringLiteral("Destino")});
    m_tabela->setSelectionMode(QAbstractItemView::NoSelection);
    m_tabela->verticalHeader()->setVisible(false);
    m_tabela->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tabela->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tabela->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tabela->verticalHeader()->setDefaultSectionSize(38);

    for (int i = 0; i < m_plano.colunas.size(); ++i) {
        const PlanoImportacao::Coluna &col = m_plano.colunas.at(i);

        auto *itemMarcar = new QTableWidgetItem;
        itemMarcar->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        itemMarcar->setCheckState(col.importar ? Qt::Checked : Qt::Unchecked);
        m_tabela->setItem(i, 0, itemMarcar);

        auto *itemNome = new QTableWidgetItem(col.nome);
        itemNome->setFlags(Qt::ItemIsEnabled);
        m_tabela->setItem(i, 1, itemNome);

        auto *combo = new QComboBox;
        combo->addItem(QStringLiteral("➕ Criar avaliação nova (peso %1, máx %2, %3º período)")
                           .arg(numero(col.peso), numero(col.notaMaxima))
                           .arg(col.periodo),
                       0);
        for (const Avaliacao &a : avaliacoesDaTurma)
            combo->addItem(QStringLiteral("Atualizar \"%1\"").arg(a.nome), a.id);
        const int idx = combo->findData(col.avaliacaoExistenteId);
        combo->setCurrentIndex(idx >= 0 ? idx : 0);
        m_tabela->setCellWidget(i, 2, combo);
    }

    auto *botoes = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botoes->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Importar"));
    botoes->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("primary"));
    botoes->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
    connect(botoes, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(botoes, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(12);
    layout->addWidget(lblResumo);
    layout->addWidget(m_tabela, 1);
    layout->addWidget(botoes);
}

PlanoImportacao ImportarNotasDialog::plano() const
{
    PlanoImportacao p = m_plano;
    for (int i = 0; i < p.colunas.size(); ++i) {
        p.colunas[i].importar = m_tabela->item(i, 0)->checkState() == Qt::Checked;
        auto *combo = qobject_cast<QComboBox *>(m_tabela->cellWidget(i, 2));
        p.colunas[i].avaliacaoExistenteId = combo ? combo->currentData().toInt() : 0;
    }
    return p;
}
