#include "ui/OcorrenciasDialog.h"

#include "core/OcorrenciaUtil.h"
#include "database/OcorrenciaRepository.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

constexpr int kRoleId = Qt::UserRole;
constexpr int kLimiteDoTexto = 2000;  // um registro é uma nota curta, não um documento

QString nomeDoTipo(const QString &id)
{
    return QString::fromUtf8(OcorrenciaUtil::tipoDe(id.toStdString()).rotulo);
}

}  // namespace

OcorrenciasDialog::OcorrenciasDialog(OcorrenciaRepository &repo, const Aluno &aluno, QWidget *parent)
    : QDialog(parent), m_repo(repo), m_aluno(aluno)
{
    setWindowTitle(QStringLiteral("Ocorrências — %1").arg(aluno.nome));
    setMinimumSize(720, 560);

    auto *titulo = new QLabel(QStringLiteral("Ocorrências de %1").arg(aluno.nome));
    titulo->setObjectName(QStringLiteral("sectionTitle"));
    titulo->setTextFormat(Qt::PlainText);  // o nome vem do usuário: nunca como HTML
    auto *dica = new QLabel(QStringLiteral("Registre elogios, problemas de conduta, dificuldades e contatos com a "
                                           "família. Entram na ficha do aluno e no painel \"Alunos em atenção\"."));
    dica->setObjectName(QStringLiteral("muted"));
    dica->setWordWrap(true);

    m_tabela = new QTableWidget(0, 3);
    m_tabela->setHorizontalHeaderLabels({QStringLiteral("Data"), QStringLiteral("Tipo"), QStringLiteral("Descrição")});
    m_tabela->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tabela->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tabela->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tabela->setAlternatingRowColors(true);
    m_tabela->setShowGrid(false);
    m_tabela->verticalHeader()->setVisible(false);
    m_tabela->verticalHeader()->setDefaultSectionSize(34);
    m_tabela->horizontalHeader()->setHighlightSections(false);
    m_tabela->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tabela->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tabela->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    m_vazio = new QLabel(QStringLiteral("Nenhuma ocorrência registrada. Use o formulário abaixo para adicionar a primeira."));
    m_vazio->setObjectName(QStringLiteral("muted"));
    m_vazio->setWordWrap(true);

    // --- Formulário ---
    m_data = new QDateEdit(QDate::currentDate());
    m_data->setCalendarPopup(true);
    m_data->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    m_data->setMaximumDate(QDate::currentDate().addDays(1));

    m_tipo = new QComboBox;
    for (const auto &tipo : OcorrenciaUtil::kTipos)
        m_tipo->addItem(QString::fromUtf8(tipo.rotulo), QString::fromLatin1(tipo.id));
    m_tipo->setCurrentIndex(m_tipo->findData(QStringLiteral("outro")));

    m_texto = new QPlainTextEdit;
    m_texto->setFixedHeight(84);
    m_texto->setPlaceholderText(QStringLiteral("O que aconteceu? (obrigatório)"));

    auto *linhaDataTipo = new QHBoxLayout;
    linhaDataTipo->addWidget(m_data);
    linhaDataTipo->addWidget(m_tipo, 1);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Data e tipo"), linhaDataTipo);
    form->addRow(QStringLiteral("Descrição *"), m_texto);

    m_btnNova = new QPushButton(QStringLiteral("Nova ocorrência"));
    m_btnSalvar = new QPushButton(QStringLiteral("Adicionar"));
    m_btnSalvar->setObjectName(QStringLiteral("primary"));
    m_btnExcluir = new QPushButton(QStringLiteral("Excluir"));
    m_btnExcluir->setObjectName(QStringLiteral("danger"));
    auto *botoesForm = new QHBoxLayout;
    botoesForm->addWidget(m_btnExcluir);
    botoesForm->addStretch(1);
    botoesForm->addWidget(m_btnNova);
    botoesForm->addWidget(m_btnSalvar);

    auto *fechar = new QDialogButtonBox(QDialogButtonBox::Close);
    fechar->button(QDialogButtonBox::Close)->setText(QStringLiteral("Fechar"));
    connect(fechar, &QDialogButtonBox::rejected, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(10);
    layout->addWidget(titulo);
    layout->addWidget(dica);
    layout->addWidget(m_tabela, 1);
    layout->addWidget(m_vazio);
    layout->addLayout(form);
    layout->addLayout(botoesForm);
    layout->addWidget(fechar);

    connect(m_tabela, &QTableWidget::itemSelectionChanged, this, &OcorrenciasDialog::carregarNoFormulario);
    connect(m_btnNova, &QPushButton::clicked, this, [this] {
        m_tabela->clearSelection();
        limparFormulario();
        m_texto->setFocus();
    });
    connect(m_btnSalvar, &QPushButton::clicked, this, &OcorrenciasDialog::salvar);
    connect(m_btnExcluir, &QPushButton::clicked, this, &OcorrenciasDialog::excluir);

    recarregar();
}

int OcorrenciasDialog::ocorrenciaSelecionadaId() const
{
    const auto linhas = m_tabela->selectionModel()->selectedRows();
    if (linhas.isEmpty())
        return 0;
    return m_tabela->item(linhas.first().row(), 0)->data(kRoleId).toInt();
}

void OcorrenciasDialog::recarregar(int selecionarId)
{
    m_tabela->blockSignals(true);
    m_tabela->setRowCount(0);
    const QList<Ocorrencia> lista = m_repo.listarPorAluno(m_aluno.id);
    int linhaParaSelecionar = -1;
    for (const Ocorrencia &o : lista) {
        const int linha = m_tabela->rowCount();
        m_tabela->insertRow(linha);
        auto *data = new QTableWidgetItem(o.data.toString(QStringLiteral("dd/MM/yyyy")));
        data->setData(kRoleId, o.id);
        auto *tipo = new QTableWidgetItem(nomeDoTipo(o.tipo));
        auto *texto = new QTableWidgetItem(QString(o.texto).replace(QLatin1Char('\n'), QLatin1Char(' ')));
        texto->setToolTip(o.texto);
        m_tabela->setItem(linha, 0, data);
        m_tabela->setItem(linha, 1, tipo);
        m_tabela->setItem(linha, 2, texto);
        if (o.id == selecionarId)
            linhaParaSelecionar = linha;
    }
    m_tabela->blockSignals(false);
    m_vazio->setVisible(lista.isEmpty());
    m_tabela->setVisible(!lista.isEmpty());

    if (linhaParaSelecionar >= 0)
        m_tabela->selectRow(linhaParaSelecionar);  // dispara carregarNoFormulario()
    else
        limparFormulario();
}

void OcorrenciasDialog::carregarNoFormulario()
{
    const int id = ocorrenciaSelecionadaId();
    const auto o = id > 0 ? m_repo.buscar(id) : std::nullopt;
    if (!o) {
        limparFormulario();
        return;
    }
    m_editandoId = o->id;
    m_data->setMaximumDate(qMax(QDate::currentDate().addDays(1), o->data));  // não impede abrir uma data futura já gravada
    m_data->setDate(o->data);
    const int indice = m_tipo->findData(OcorrenciaUtil::idValido(o->tipo.toStdString()) ? o->tipo : QStringLiteral("outro"));
    m_tipo->setCurrentIndex(qMax(0, indice));
    m_texto->setPlainText(o->texto);
    atualizarEstado();
}

void OcorrenciasDialog::limparFormulario()
{
    m_editandoId = 0;
    m_data->setMaximumDate(QDate::currentDate().addDays(1));
    m_data->setDate(QDate::currentDate());
    m_tipo->setCurrentIndex(qMax(0, m_tipo->findData(QStringLiteral("outro"))));
    m_texto->clear();
    atualizarEstado();
}

void OcorrenciasDialog::atualizarEstado()
{
    m_btnSalvar->setText(m_editandoId > 0 ? QStringLiteral("Salvar alterações") : QStringLiteral("Adicionar"));
    m_btnExcluir->setEnabled(m_editandoId > 0);
}

void OcorrenciasDialog::salvar()
{
    const QString texto = m_texto->toPlainText().trimmed();
    if (texto.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Campo obrigatório"),
                             QStringLiteral("Descreva o que aconteceu antes de salvar."));
        m_texto->setFocus();
        return;
    }
    if (texto.size() > kLimiteDoTexto) {
        QMessageBox::warning(this, QStringLiteral("Texto muito longo"),
                             QStringLiteral("A descrição pode ter até %1 caracteres (agora tem %2).")
                                 .arg(kLimiteDoTexto)
                                 .arg(texto.size()));
        return;
    }

    Ocorrencia o;
    o.id = m_editandoId;
    o.alunoId = m_aluno.id;
    o.data = m_data->date();
    o.tipo = m_tipo->currentData().toString();
    o.texto = texto;

    int id = m_editandoId;
    if (m_editandoId > 0) {
        if (!m_repo.atualizar(o)) {
            QMessageBox::critical(this, QStringLiteral("Não foi possível salvar"), m_repo.ultimoErro());
            return;
        }
    } else {
        id = m_repo.inserir(o);
        if (id == 0) {
            QMessageBox::critical(this, QStringLiteral("Não foi possível salvar"), m_repo.ultimoErro());
            return;
        }
    }
    recarregar(id);
}

void OcorrenciasDialog::excluir()
{
    if (m_editandoId <= 0)
        return;
    if (QMessageBox::question(this, QStringLiteral("Excluir ocorrência"),
                              QStringLiteral("Excluir esta ocorrência? Esta ação não pode ser desfeita."),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    if (!m_repo.remover(m_editandoId)) {
        QMessageBox::critical(this, QStringLiteral("Não foi possível excluir"), m_repo.ultimoErro());
        return;
    }
    recarregar();
}
