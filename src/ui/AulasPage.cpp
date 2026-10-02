#include "ui/AulasPage.h"

#include "database/AulaRepository.h"
#include "database/Repositorios.h"
#include "database/TurmaRepository.h"
#include "ui/AnexosWidget.h"
#include "ui/EstadoVazio.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

AulasPage::AulasPage(Repositorios &repos, QWidget *parent)
    : QWidget(parent), m_aulas(repos.aulas), m_turmas(repos.turmas)
{
    auto *raiz = new QVBoxLayout(this);
    raiz->setContentsMargins(32, 28, 32, 24);
    raiz->setSpacing(6);

    auto *titulo = new QLabel(QStringLiteral("Aulas"));
    titulo->setObjectName(QStringLiteral("pageTitle"));
    auto *subtitulo = new QLabel(QStringLiteral(
        "Planeje cada aula (tema, objetivos, materiais) e anexe as apresentações: elas abrem no programa padrão do computador."));
    subtitulo->setObjectName(QStringLiteral("pageSubtitle"));
    subtitulo->setWordWrap(true);
    raiz->addWidget(titulo);
    raiz->addWidget(subtitulo);
    raiz->addSpacing(12);

    auto *divisor = new QSplitter(Qt::Horizontal);
    divisor->setChildrenCollapsible(false);

    // ---------------- Lista de planos ----------------
    auto *painelLista = new QWidget;
    auto *ll = new QVBoxLayout(painelLista);
    ll->setContentsMargins(0, 0, 12, 0);
    auto *barra = new QHBoxLayout;
    m_filtroTurma = new QComboBox;
    m_btnNovo = new QPushButton(QStringLiteral("+ Novo plano"));
    m_btnNovo->setObjectName(QStringLiteral("primary"));
    barra->addWidget(m_filtroTurma, 1);
    barra->addWidget(m_btnNovo);
    ll->addLayout(barra);
    m_lista = new QListWidget;
    m_lista->setAlternatingRowColors(true);
    ll->addWidget(m_lista, 1);
    EstadoVazio::sobre(m_lista, QStringLiteral("aulas"), QStringLiteral("Nenhum plano de aula"),
                       QStringLiteral("Os planos que você criar aparecem aqui."));

    // ---------------- Editor ----------------
    auto *painelEditor = new QWidget;
    auto *le = new QVBoxLayout(painelEditor);
    le->setContentsMargins(12, 0, 0, 0);

    m_vazio = new EstadoVazio(QStringLiteral("aulas"), QStringLiteral("Nenhum plano aberto"),
                              QStringLiteral("Selecione um plano da lista ou clique em \"+ Novo plano\"."));
    le->addWidget(m_vazio, 1);

    m_editor = new QWidget;
    auto *ed = new QVBoxLayout(m_editor);
    ed->setContentsMargins(0, 0, 0, 0);

    m_turma = new QComboBox;
    m_data = new QDateEdit(QDate::currentDate());
    m_data->setCalendarPopup(true);
    m_data->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    m_tema = new QLineEdit;
    m_tema->setPlaceholderText(QStringLiteral("Tema da aula"));
    m_objetivos = new QPlainTextEdit;
    m_objetivos->setPlaceholderText(QStringLiteral("O que os alunos devem aprender nesta aula?"));
    m_objetivos->setFixedHeight(90);
    m_materiais = new QPlainTextEdit;
    m_materiais->setPlaceholderText(QStringLiteral("Livros, slides, atividades, recursos…"));
    m_materiais->setFixedHeight(70);
    m_observacoes = new QPlainTextEdit;
    m_observacoes->setPlaceholderText(QStringLiteral("Como foi a aula, o que ajustar na próxima…"));
    m_observacoes->setFixedHeight(70);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Turma"), m_turma);
    form->addRow(QStringLiteral("Data"), m_data);
    form->addRow(QStringLiteral("Tema"), m_tema);
    form->addRow(QStringLiteral("Objetivos"), m_objetivos);
    form->addRow(QStringLiteral("Materiais"), m_materiais);
    form->addRow(QStringLiteral("Observações"), m_observacoes);
    ed->addLayout(form);

    auto *lblAnexos = new QLabel(QStringLiteral("Apresentações e arquivos"));
    lblAnexos->setObjectName(QStringLiteral("sectionTitle"));
    ed->addWidget(lblAnexos);
    m_anexos = new AnexosWidget(repos.anexos);
    ed->addWidget(m_anexos, 1);

    auto *rodape = new QHBoxLayout;
    m_estado = new QLabel;
    m_estado->setObjectName(QStringLiteral("muted"));
    m_btnExcluir = new QPushButton(QStringLiteral("Excluir plano"));
    m_btnExcluir->setObjectName(QStringLiteral("danger"));
    m_btnSalvar = new QPushButton(QStringLiteral("Salvar"));
    m_btnSalvar->setObjectName(QStringLiteral("primary"));
    rodape->addWidget(m_estado, 1);
    rodape->addWidget(m_btnExcluir);
    rodape->addWidget(m_btnSalvar);
    ed->addLayout(rodape);
    le->addWidget(m_editor, 1);

    divisor->addWidget(painelLista);
    divisor->addWidget(painelEditor);
    divisor->setStretchFactor(0, 2);
    divisor->setStretchFactor(1, 3);
    raiz->addWidget(divisor, 1);

    // ---------------- Conexões ----------------
    connect(m_btnNovo, &QPushButton::clicked, this, &AulasPage::novoPlano);
    connect(m_btnSalvar, &QPushButton::clicked, this, [this] { salvar(); });
    connect(m_btnExcluir, &QPushButton::clicked, this, &AulasPage::excluir);
    connect(m_filtroTurma, &QComboBox::currentIndexChanged, this, [this] {
        if (m_carregando)
            return;  // a própria tela está reconstruindo a lista de turmas
        if (confirmarDescarteSeNecessario())
            recarregarLista();
    });

    // Qualquer alteração nos campos marca o plano como "não salvo".
    auto sujar = [this] { marcarSujo(true); };
    connect(m_turma, &QComboBox::currentIndexChanged, this, sujar);
    connect(m_data, &QDateEdit::dateChanged, this, sujar);
    connect(m_tema, &QLineEdit::textChanged, this, sujar);
    connect(m_objetivos, &QPlainTextEdit::textChanged, this, sujar);
    connect(m_materiais, &QPlainTextEdit::textChanged, this, sujar);
    connect(m_observacoes, &QPlainTextEdit::textChanged, this, sujar);

    connect(m_lista, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *atual, QListWidgetItem *anterior) {
        if (m_carregando)
            return;
        const int novoId = atual ? atual->data(Qt::UserRole).toInt() : 0;
        if (novoId == m_aulaId)
            return;
        if (!confirmarDescarteSeNecessario()) {
            // Usuário cancelou: volta a seleção para o plano que estava aberto.
            m_carregando = true;
            m_lista->setCurrentItem(anterior);
            m_carregando = false;
            return;
        }
        carregarNoEditor(novoId);
    });

    limparEditor();
}

void AulasPage::showEvent(QShowEvent *evento)
{
    QWidget::showEvent(evento);
    if (!m_sujo)
        recarregarTurmas();
}

// ============================================================================

void AulasPage::recarregarTurmas()
{
    const int filtroAnterior = m_filtroTurma->currentData().toInt();
    const QList<Turma> turmas = m_turmas.listar(false);

    m_carregando = true;
    m_filtroTurma->clear();
    m_filtroTurma->addItem(QStringLiteral("Todas as turmas"), 0);
    m_turma->clear();
    for (const Turma &t : turmas) {
        m_filtroTurma->addItem(t.nome, t.id);
        m_turma->addItem(t.nome, t.id);
    }
    const int idx = m_filtroTurma->findData(filtroAnterior);
    m_filtroTurma->setCurrentIndex(idx >= 0 ? idx : 0);
    m_carregando = false;

    m_btnNovo->setEnabled(!turmas.isEmpty());
    recarregarLista(m_aulaId);
}

void AulasPage::recarregarLista(int selecionarId)
{
    m_carregando = true;
    m_lista->clear();
    QListWidgetItem *aSelecionar = nullptr;
    for (const Aula &a : m_aulas.listar(m_filtroTurma->currentData().toInt())) {
        auto *item = new QListWidgetItem(QStringLiteral("%1  ·  %2\n%3")
                                             .arg(a.data.toString(QStringLiteral("dd/MM/yyyy")), a.turmaNome,
                                                  a.tema.isEmpty() ? QStringLiteral("(sem tema)") : a.tema));
        item->setData(Qt::UserRole, a.id);
        m_lista->addItem(item);
        if (a.id == selecionarId)
            aSelecionar = item;
    }
    if (aSelecionar)
        m_lista->setCurrentItem(aSelecionar);
    m_carregando = false;

    if (aSelecionar)
        carregarNoEditor(selecionarId);
    else
        limparEditor();
}

void AulasPage::carregarNoEditor(int aulaId)
{
    const auto aula = aulaId > 0 ? m_aulas.buscar(aulaId) : std::nullopt;
    if (!aula) {
        limparEditor();
        return;
    }

    m_carregando = true;
    m_aulaId = aula->id;
    const int idxTurma = m_turma->findData(aula->turmaId);
    m_turma->setCurrentIndex(idxTurma >= 0 ? idxTurma : 0);
    m_data->setDate(aula->data.isValid() ? aula->data : QDate::currentDate());
    m_tema->setText(aula->tema);
    m_objetivos->setPlainText(aula->objetivos);
    m_materiais->setPlainText(aula->materiais);
    m_observacoes->setPlainText(aula->observacoes);
    m_carregando = false;

    m_anexos->definirContexto(aula->turmaId, aula->id);
    m_editor->setVisible(true);
    m_vazio->setVisible(false);
    marcarSujo(false);
}

void AulasPage::limparEditor()
{
    m_aulaId = 0;
    m_anexos->definirContexto(0, 0);
    m_editor->setVisible(false);
    m_vazio->setVisible(true);
    marcarSujo(false);
}

void AulasPage::marcarSujo(bool sujo)
{
    if (sujo && m_carregando)
        return;
    m_sujo = sujo;
    m_btnSalvar->setEnabled(sujo);
    m_estado->setText(sujo ? QStringLiteral("Alterações não salvas") : QString());
}

bool AulasPage::confirmarDescarteSeNecessario()
{
    if (!m_sujo)
        return true;

    QMessageBox caixa(this);
    caixa.setIcon(QMessageBox::Question);
    caixa.setWindowTitle(QStringLiteral("Alterações não salvas"));
    caixa.setText(QStringLiteral("Este plano de aula tem alterações não salvas."));
    QPushButton *salvarBtn = caixa.addButton(QStringLiteral("Salvar"), QMessageBox::AcceptRole);
    QPushButton *descartarBtn = caixa.addButton(QStringLiteral("Descartar"), QMessageBox::DestructiveRole);
    caixa.addButton(QStringLiteral("Cancelar"), QMessageBox::RejectRole);
    caixa.exec();

    if (caixa.clickedButton() == salvarBtn)
        return salvar();
    if (caixa.clickedButton() == descartarBtn) {
        marcarSujo(false);
        return true;
    }
    return false;
}

bool AulasPage::salvar()
{
    if (m_aulaId == 0)
        return true;

    Aula a;
    a.id = m_aulaId;
    a.turmaId = m_turma->currentData().toInt();
    a.data = m_data->date();
    a.tema = m_tema->text().trimmed();
    a.objetivos = m_objetivos->toPlainText().trimmed();
    a.materiais = m_materiais->toPlainText().trimmed();
    a.observacoes = m_observacoes->toPlainText().trimmed();

    if (a.turmaId <= 0) {
        QMessageBox::warning(this, QStringLiteral("Campo obrigatório"), QStringLiteral("Escolha a turma."));
        return false;
    }
    if (!m_aulas.atualizar(a)) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_aulas.ultimoErro());
        return false;
    }
    marcarSujo(false);
    recarregarLista(a.id);
    return true;
}

void AulasPage::novoPlano()
{
    if (!confirmarDescarteSeNecessario())
        return;

    // O plano é criado já no banco (com dados padrão) para poder receber anexos.
    Aula a;
    a.turmaId = m_filtroTurma->currentData().toInt();
    if (a.turmaId == 0 && m_turma->count() > 0)
        a.turmaId = m_turma->itemData(0).toInt();
    a.data = QDate::currentDate();
    a.tema = QStringLiteral("Nova aula");

    const int id = m_aulas.inserir(a);
    if (id == 0) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_aulas.ultimoErro());
        return;
    }
    recarregarLista(id);
    m_tema->setFocus();
    m_tema->selectAll();
}

void AulasPage::excluir()
{
    if (m_aulaId == 0)
        return;
    const auto resp = QMessageBox::question(
        this, QStringLiteral("Excluir plano de aula"),
        QStringLiteral("Excluir este plano de aula?\n\nA lista de anexos dele também é apagada "
                       "(os arquivos em si continuam no computador)."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (resp != QMessageBox::Yes)
        return;

    if (!m_aulas.remover(m_aulaId)) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_aulas.ultimoErro());
        return;
    }
    m_aulaId = 0;
    marcarSujo(false);
    recarregarLista();
}

void AulasPage::selecionarAula(int aulaId)
{
    if (!confirmarDescarteSeNecessario())
        return;
    const auto aula = m_aulas.buscar(aulaId);
    if (!aula)
        return;
    recarregarTurmas();
    // Mostra todas as turmas para garantir que o plano aparece na lista.
    m_carregando = true;
    m_filtroTurma->setCurrentIndex(0);
    m_carregando = false;
    recarregarLista(aulaId);
}
