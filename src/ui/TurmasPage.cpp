#include "ui/TurmasPage.h"
#include "ui/ThemeManager.h"

#include "database/AlunoRepository.h"
#include "database/AnotacaoRepository.h"
#include "database/Repositorios.h"
#include "database/TurmaRepository.h"
#include "ui/AlunoDialog.h"
#include "ui/AnexosWidget.h"
#include "ui/TurmaDialog.h"

#include <QCheckBox>
#include <QListWidget>
#include <QTabWidget>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

// Guardamos o id do registro no primeiro item de cada linha (role do usuário).
constexpr int kRoleId = Qt::UserRole;

// Quadradinho colorido mostrado ao lado do nome da turma.
QIcon iconeDaCor(const QString &cor)
{
    QPixmap pm(14, 14);
    pm.fill(ThemeManager::corDaTurma(cor));
    return QIcon(pm);
}

QTableWidgetItem *novoItem(const QString &texto)
{
    auto *item = new QTableWidgetItem(texto);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);  // edição só pelos diálogos
    return item;
}

void configurarTabela(QTableWidget *t)
{
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setAlternatingRowColors(true);
    t->setShowGrid(false);
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(34);
    t->horizontalHeader()->setHighlightSections(false);
}

}  // namespace

TurmasPage::TurmasPage(Repositorios &repos, QWidget *parent)
    : QWidget(parent), m_turmas(repos.turmas), m_alunos(repos.alunos), m_anotacoes(repos.anotacoes)
{
    auto *raiz = new QVBoxLayout(this);
    raiz->setContentsMargins(32, 28, 32, 24);
    raiz->setSpacing(6);

    auto *titulo = new QLabel(QStringLiteral("Turmas"));
    titulo->setObjectName(QStringLiteral("pageTitle"));
    auto *subtitulo = new QLabel(QStringLiteral("Cadastre suas turmas e os alunos de cada uma."));
    subtitulo->setObjectName(QStringLiteral("pageSubtitle"));
    raiz->addWidget(titulo);
    raiz->addWidget(subtitulo);
    raiz->addSpacing(14);

    auto *divisor = new QSplitter(Qt::Horizontal);
    divisor->setChildrenCollapsible(false);

    // ------------------------- Painel de turmas -------------------------
    auto *painelTurmas = new QWidget;
    auto *lt = new QVBoxLayout(painelTurmas);
    lt->setContentsMargins(0, 0, 12, 0);

    auto *barraTurmas = new QHBoxLayout;
    auto *lblTurmas = new QLabel(QStringLiteral("Minhas turmas"));
    lblTurmas->setObjectName(QStringLiteral("sectionTitle"));
    m_btnNovaTurma = new QPushButton(QStringLiteral("+ Nova turma"));
    m_btnNovaTurma->setObjectName(QStringLiteral("primary"));
    barraTurmas->addWidget(lblTurmas);
    barraTurmas->addStretch(1);
    barraTurmas->addWidget(m_btnNovaTurma);
    lt->addLayout(barraTurmas);

    m_tabelaTurmas = new QTableWidget(0, 3);
    m_tabelaTurmas->setHorizontalHeaderLabels({QStringLiteral("Turma"), QStringLiteral("Ano"),
                                               QStringLiteral("Alunos")});
    configurarTabela(m_tabelaTurmas);
    m_tabelaTurmas->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tabelaTurmas->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tabelaTurmas->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    lt->addWidget(m_tabelaTurmas, 1);

    auto *rodapeTurmas = new QHBoxLayout;
    m_mostrarArquivadas = new QCheckBox(QStringLiteral("Mostrar arquivadas"));
    m_btnEditarTurma = new QPushButton(QStringLiteral("Editar"));
    m_btnExcluirTurma = new QPushButton(QStringLiteral("Excluir"));
    m_btnExcluirTurma->setObjectName(QStringLiteral("danger"));
    rodapeTurmas->addWidget(m_mostrarArquivadas);
    rodapeTurmas->addStretch(1);
    rodapeTurmas->addWidget(m_btnEditarTurma);
    rodapeTurmas->addWidget(m_btnExcluirTurma);
    lt->addLayout(rodapeTurmas);

    // ------------------------- Painel de alunos -------------------------
    auto *painelAlunos = new QWidget;
    auto *la = new QVBoxLayout(painelAlunos);
    la->setContentsMargins(12, 0, 0, 0);

    auto *barraAlunos = new QHBoxLayout;
    m_tituloAlunos = new QLabel(QStringLiteral("Alunos"));
    m_tituloAlunos->setObjectName(QStringLiteral("sectionTitle"));
    m_busca = new QLineEdit;
    m_busca->setPlaceholderText(QStringLiteral("Buscar aluno por nome, matrícula ou e-mail..."));
    m_busca->setClearButtonEnabled(true);
    m_busca->setMinimumWidth(260);
    m_btnNovoAluno = new QPushButton(QStringLiteral("+ Novo aluno"));
    m_btnNovoAluno->setObjectName(QStringLiteral("primary"));
    barraAlunos->addWidget(m_tituloAlunos);
    barraAlunos->addStretch(1);
    barraAlunos->addWidget(m_busca);
    barraAlunos->addWidget(m_btnNovoAluno);
    la->addLayout(barraAlunos);

    m_tabelaAlunos = new QTableWidget(0, 5);
    m_tabelaAlunos->setHorizontalHeaderLabels({QStringLiteral("Matrícula"), QStringLiteral("Nome"),
                                               QStringLiteral("E-mail"), QStringLiteral("Nascimento"),
                                               QStringLiteral("Situação")});
    configurarTabela(m_tabelaAlunos);
    m_tabelaAlunos->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tabelaAlunos->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tabelaAlunos->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tabelaAlunos->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tabelaAlunos->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    la->addWidget(m_tabelaAlunos, 1);

    auto *rodapeAlunos = new QHBoxLayout;
    m_btnEditarAluno = new QPushButton(QStringLiteral("Editar"));
    m_btnExcluirAluno = new QPushButton(QStringLiteral("Excluir"));
    m_btnExcluirAluno->setObjectName(QStringLiteral("danger"));
    rodapeAlunos->addStretch(1);
    rodapeAlunos->addWidget(m_btnEditarAluno);
    rodapeAlunos->addWidget(m_btnExcluirAluno);
    la->addLayout(rodapeAlunos);

    // ------------------------- Abas da turma -------------------------
    // Alunos | Arquivos (apresentações e documentos) | Anotações ligadas à turma
    m_abas = new QTabWidget;
    m_abas->addTab(painelAlunos, QStringLiteral("Alunos"));

    m_anexos = new AnexosWidget(repos.anexos);
    auto *abaArquivos = new QWidget;
    auto *la2 = new QVBoxLayout(abaArquivos);
    la2->setContentsMargins(0, 12, 0, 0);
    la2->addWidget(m_anexos);
    m_abas->addTab(abaArquivos, QStringLiteral("Arquivos"));

    m_listaNotas = new QListWidget;
    m_listaNotas->setAlternatingRowColors(true);
    auto *abaNotas = new QWidget;
    auto *la3 = new QVBoxLayout(abaNotas);
    la3->setContentsMargins(0, 12, 0, 0);
    auto *dicaNotas = new QLabel(QStringLiteral("Anotações ligadas a esta turma. Duplo clique para abrir no editor."));
    dicaNotas->setObjectName(QStringLiteral("muted"));
    la3->addWidget(dicaNotas);
    la3->addWidget(m_listaNotas, 1);
    m_abas->addTab(abaNotas, QStringLiteral("Anotações"));
    connect(m_listaNotas, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        emit abrirAnotacaoSolicitada(item->data(Qt::UserRole).toInt());
    });

    divisor->addWidget(painelTurmas);
    divisor->addWidget(m_abas);
    divisor->setStretchFactor(0, 2);
    divisor->setStretchFactor(1, 3);
    raiz->addWidget(divisor, 1);

    // ------------------------------ Conexões ------------------------------
    // Os quadradinhos de cor das turmas acompanham o tema.
    connect(&ThemeManager::notificador(), &ThemeNotifier::temaMudou, this, [this] {
        if (isVisible())
            recarregarTurmas(turmaSelecionadaId());
    });
    connect(m_btnNovaTurma, &QPushButton::clicked, this, &TurmasPage::novaTurma);
    connect(m_btnEditarTurma, &QPushButton::clicked, this, &TurmasPage::editarTurma);
    connect(m_btnExcluirTurma, &QPushButton::clicked, this, &TurmasPage::excluirTurma);
    connect(m_mostrarArquivadas, &QCheckBox::toggled, this, [this] { recarregarTurmas(turmaSelecionadaId()); });
    connect(m_tabelaTurmas, &QTableWidget::itemSelectionChanged, this, [this] {
        recarregarAlunos();
        atualizarAbasDaTurma();
        atualizarEstadoBotoes();
    });
    connect(m_tabelaTurmas, &QTableWidget::cellDoubleClicked, this, [this] { editarTurma(); });

    connect(m_btnNovoAluno, &QPushButton::clicked, this, &TurmasPage::novoAluno);
    connect(m_btnEditarAluno, &QPushButton::clicked, this, &TurmasPage::editarAluno);
    connect(m_btnExcluirAluno, &QPushButton::clicked, this, &TurmasPage::excluirAluno);
    connect(m_busca, &QLineEdit::textChanged, this, [this] { recarregarAlunos(alunoSelecionadoId()); });
    connect(m_tabelaAlunos, &QTableWidget::itemSelectionChanged, this, &TurmasPage::atualizarEstadoBotoes);
    connect(m_tabelaAlunos, &QTableWidget::cellDoubleClicked, this, [this] { editarAluno(); });

    recarregarTurmas();
    atualizarEstadoBotoes();
}

// ============================================================================
// Turmas
// ============================================================================

int TurmasPage::turmaSelecionadaId() const
{
    const auto linhas = m_tabelaTurmas->selectionModel()->selectedRows();
    if (linhas.isEmpty())
        return 0;
    return m_tabelaTurmas->item(linhas.first().row(), 0)->data(kRoleId).toInt();
}

void TurmasPage::recarregarTurmas(int selecionarId)
{
    const QList<Turma> lista = m_turmas.listar(m_mostrarArquivadas->isChecked());

    // Evita recarregar os alunos a cada linha inserida.
    m_tabelaTurmas->blockSignals(true);
    m_tabelaTurmas->setRowCount(0);

    int linhaParaSelecionar = -1;
    for (const Turma &t : lista) {
        const int linha = m_tabelaTurmas->rowCount();
        m_tabelaTurmas->insertRow(linha);

        QString nome = t.nome;
        if (!t.disciplina.isEmpty())
            nome += QStringLiteral(" — ") + t.disciplina;
        if (t.arquivada)
            nome += QStringLiteral(" (arquivada)");

        auto *itemNome = novoItem(nome);
        itemNome->setIcon(iconeDaCor(t.cor));
        itemNome->setData(kRoleId, t.id);
        m_tabelaTurmas->setItem(linha, 0, itemNome);
        m_tabelaTurmas->setItem(linha, 1, novoItem(QString::number(t.anoLetivo)));
        m_tabelaTurmas->setItem(linha, 2, novoItem(QString::number(t.totalAlunos)));

        if (t.id == selecionarId)
            linhaParaSelecionar = linha;
    }
    m_tabelaTurmas->blockSignals(false);

    if (linhaParaSelecionar < 0 && !lista.isEmpty())
        linhaParaSelecionar = 0;
    if (linhaParaSelecionar >= 0)
        m_tabelaTurmas->selectRow(linhaParaSelecionar);  // dispara itemSelectionChanged
    else {
        recarregarAlunos();
        atualizarAbasDaTurma();
        atualizarEstadoBotoes();
    }
}

// Atualiza as abas "Arquivos" e "Anotações" para a turma selecionada.
void TurmasPage::atualizarAbasDaTurma()
{
    const int turmaId = turmaSelecionadaId();
    m_anexos->definirContexto(turmaId, 0);  // 0 = todos os anexos da turma

    m_listaNotas->clear();
    if (turmaId == 0)
        return;
    AnotacaoRepository::Filtro filtro;
    filtro.turmaId = turmaId;
    for (const AnotacaoResumo &r : m_anotacoes.listar(filtro)) {
        QString texto = r.titulo.isEmpty() ? QStringLiteral("(sem título)") : r.titulo;
        if (!r.tags.isEmpty())
            texto += QStringLiteral("   #") + r.tags.join(QStringLiteral(" #"));
        auto *item = new QListWidgetItem(texto);
        item->setData(Qt::UserRole, r.id);
        item->setToolTip(r.trecho);
        m_listaNotas->addItem(item);
    }
}

void TurmasPage::selecionarTurma(int turmaId, int alunoId)
{
    const auto turma = m_turmas.buscar(turmaId);
    if (!turma)
        return;
    if (turma->arquivada && !m_mostrarArquivadas->isChecked())
        m_mostrarArquivadas->setChecked(true);  // o sinal toggled recarrega a lista
    m_busca->clear();
    recarregarTurmas(turmaId);
    m_abas->setCurrentIndex(0);
    if (alunoId > 0)
        recarregarAlunos(alunoId);
}

void TurmasPage::novaTurma()
{
    TurmaDialog dlg(nullptr, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    Turma t = dlg.turma();
    const int id = m_turmas.inserir(t);
    if (id == 0) {
        mostrarErro(QStringLiteral("Não foi possível criar a turma"), m_turmas.ultimoErro());
        return;
    }
    recarregarTurmas(id);
}

void TurmasPage::editarTurma()
{
    const int id = turmaSelecionadaId();
    const auto existente = m_turmas.buscar(id);
    if (!existente)
        return;

    TurmaDialog dlg(&*existente, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    if (!m_turmas.atualizar(dlg.turma())) {
        mostrarErro(QStringLiteral("Não foi possível salvar a turma"), m_turmas.ultimoErro());
        return;
    }
    recarregarTurmas(id);
}

void TurmasPage::excluirTurma()
{
    const int id = turmaSelecionadaId();
    const auto turma = m_turmas.buscar(id);
    if (!turma)
        return;

    const QString pergunta =
        QStringLiteral("Excluir a turma \"%1\"?\n\nTambém serão apagados os %2 aluno(s) dela, "
                       "com notas e frequência. Esta ação não pode ser desfeita.\n\n"
                       "Dica: para só ocultar a turma, use Editar → \"Turma arquivada\".")
            .arg(turma->nome)
            .arg(turma->totalAlunos);
    if (QMessageBox::question(this, QStringLiteral("Excluir turma"), pergunta,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    if (!m_turmas.remover(id)) {
        mostrarErro(QStringLiteral("Não foi possível excluir a turma"), m_turmas.ultimoErro());
        return;
    }
    recarregarTurmas();
}

// ============================================================================
// Alunos
// ============================================================================

int TurmasPage::alunoSelecionadoId() const
{
    const auto linhas = m_tabelaAlunos->selectionModel()->selectedRows();
    if (linhas.isEmpty())
        return 0;
    return m_tabelaAlunos->item(linhas.first().row(), 0)->data(kRoleId).toInt();
}

void TurmasPage::recarregarAlunos(int selecionarId)
{
    m_tabelaAlunos->blockSignals(true);
    m_tabelaAlunos->setRowCount(0);

    const int turmaId = turmaSelecionadaId();
    if (turmaId == 0) {
        m_tituloAlunos->setText(QStringLiteral("Alunos"));
        m_tabelaAlunos->blockSignals(false);
        atualizarEstadoBotoes();
        return;
    }

    const auto turma = m_turmas.buscar(turmaId);
    m_tituloAlunos->setText(QStringLiteral("Alunos de %1").arg(turma ? turma->nome : QString()));

    const QList<Aluno> lista = m_alunos.listarPorTurma(turmaId, m_busca->text());
    int linhaParaSelecionar = -1;
    for (const Aluno &a : lista) {
        const int linha = m_tabelaAlunos->rowCount();
        m_tabelaAlunos->insertRow(linha);

        auto *itemMatricula = novoItem(a.matricula);
        itemMatricula->setData(kRoleId, a.id);
        m_tabelaAlunos->setItem(linha, 0, itemMatricula);
        m_tabelaAlunos->setItem(linha, 1, novoItem(a.nome));
        m_tabelaAlunos->setItem(linha, 2, novoItem(a.email));
        m_tabelaAlunos->setItem(linha, 3, novoItem(a.dataNascimento.isValid()
                                                       ? a.dataNascimento.toString(QStringLiteral("dd/MM/yyyy"))
                                                       : QString()));
        m_tabelaAlunos->setItem(linha, 4, novoItem(a.ativo ? QStringLiteral("Ativo")
                                                           : QStringLiteral("Inativo")));
        if (a.id == selecionarId)
            linhaParaSelecionar = linha;
    }
    m_tabelaAlunos->blockSignals(false);

    if (linhaParaSelecionar >= 0)
        m_tabelaAlunos->selectRow(linhaParaSelecionar);
    atualizarEstadoBotoes();
}

void TurmasPage::novoAluno()
{
    const int turmaId = turmaSelecionadaId();
    if (turmaId == 0)
        return;

    AlunoDialog dlg(turmaId, nullptr, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const int id = m_alunos.inserir(dlg.aluno());
    if (id == 0) {
        mostrarErro(QStringLiteral("Não foi possível cadastrar o aluno"), m_alunos.ultimoErro());
        return;
    }
    recarregarTurmas(turmaId);  // atualiza também o contador de alunos da turma
    recarregarAlunos(id);
}

void TurmasPage::editarAluno()
{
    const int id = alunoSelecionadoId();
    const auto existente = m_alunos.buscar(id);
    if (!existente)
        return;

    AlunoDialog dlg(existente->turmaId, &*existente, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    if (!m_alunos.atualizar(dlg.aluno())) {
        mostrarErro(QStringLiteral("Não foi possível salvar o aluno"), m_alunos.ultimoErro());
        return;
    }
    recarregarTurmas(existente->turmaId);
    recarregarAlunos(id);
}

void TurmasPage::excluirAluno()
{
    const int id = alunoSelecionadoId();
    const auto aluno = m_alunos.buscar(id);
    if (!aluno)
        return;

    const QString pergunta =
        QStringLiteral("Excluir o aluno \"%1\"?\n\nSuas notas e frequência também serão apagadas. "
                       "Esta ação não pode ser desfeita.\n\n"
                       "Dica: para manter o histórico, use Editar e desmarque \"Aluno ativo\".")
            .arg(aluno->nome);
    if (QMessageBox::question(this, QStringLiteral("Excluir aluno"), pergunta,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    if (!m_alunos.remover(id)) {
        mostrarErro(QStringLiteral("Não foi possível excluir o aluno"), m_alunos.ultimoErro());
        return;
    }
    recarregarTurmas(aluno->turmaId);
}

// ============================================================================

void TurmasPage::atualizarEstadoBotoes()
{
    const bool temTurma = turmaSelecionadaId() != 0;
    const bool temAluno = alunoSelecionadoId() != 0;
    m_btnEditarTurma->setEnabled(temTurma);
    m_btnExcluirTurma->setEnabled(temTurma);
    m_btnNovoAluno->setEnabled(temTurma);
    m_busca->setEnabled(temTurma);
    m_btnEditarAluno->setEnabled(temAluno);
    m_btnExcluirAluno->setEnabled(temAluno);
}

void TurmasPage::mostrarErro(const QString &titulo, const QString &detalhe)
{
    QMessageBox::critical(this, titulo, detalhe);
}
