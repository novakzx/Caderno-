#include "ui/MainWindow.h"

#include "core/BuildInfo.h"
#include "database/Repositorios.h"
#include "services/BackupService.h"
#include "ui/AnexosWidget.h"
#include "ui/AnotacoesPage.h"
#include "ui/AulasPage.h"
#include "ui/BackupDialog.h"
#include "ui/BuscaDialog.h"
#include "ui/CalendarioPage.h"
#include "ui/FrequenciaPage.h"
#include "ui/HojePage.h"
#include "ui/HorarioPage.h"
#include "ui/NotasPage.h"
#include "ui/RelatoriosPage.h"
#include "ui/TarefasPage.h"
#include "ui/ThemeManager.h"
#include "ui/TurmasPage.h"

#include <QButtonGroup>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kIntervaloBackupHoras = BackupService::kIntervaloPadraoHoras;  // backup automático: a cada 24 h
constexpr int kIntervaloBackupAoFecharHoras = 12;                            // e ao fechar, se o último tiver mais de 12 h
}  // namespace

MainWindow::MainWindow(Repositorios &repos, QWidget *parent) : QMainWindow(parent), m_repos(repos)
{
    setWindowTitle(QStringLiteral("Professor Organizado — %1").arg(identificacaoDoBuild()));
    resize(1280, 820);
    setMinimumSize(1000, 680);

    auto *central = new QWidget;
    auto *layoutRaiz = new QHBoxLayout(central);
    layoutRaiz->setContentsMargins(0, 0, 0, 0);
    layoutRaiz->setSpacing(0);

    // --- Barra lateral ---
    auto *barra = new QWidget;
    barra->setObjectName(QStringLiteral("sidebar"));
    barra->setFixedWidth(220);
    // QWidget "puro" só pinta o fundo do QSS com este atributo ligado.
    barra->setAttribute(Qt::WA_StyledBackground, true);
    construirBarraLateral(barra);

    // --- Área de conteúdo ---
    m_paginas = new QStackedWidget;

    layoutRaiz->addWidget(barra);
    layoutRaiz->addWidget(m_paginas, 1);
    setCentralWidget(central);

    // --- Seções (na ordem da barra lateral) ---
    adicionarSecao(QStringLiteral("🏠  Hoje"), new HojePage(repos.agenda, repos.tarefas));

    m_paginaTurmas = new TurmasPage(repos);
    adicionarSecao(QStringLiteral("👥  Turmas"), m_paginaTurmas);

    adicionarSecao(QStringLiteral("📊  Notas"),
                   new NotasPage(repos.turmas, repos.alunos, repos.avaliacoes, repos.notas));
    adicionarSecao(QStringLiteral("📋  Frequência"), new FrequenciaPage(repos));
    adicionarSecao(QStringLiteral("🗓️  Horário"), new HorarioPage(repos.horarios, repos.turmas));

    m_paginaAulas = new AulasPage(repos);
    adicionarSecao(QStringLiteral("📚  Aulas"), m_paginaAulas);

    m_paginaAnotacoes = new AnotacoesPage(repos);
    adicionarSecao(QStringLiteral("📝  Anotações"), m_paginaAnotacoes);

    m_paginaTarefas = new TarefasPage(repos);
    adicionarSecao(QStringLiteral("✅  Tarefas"), m_paginaTarefas);

    m_paginaCalendario = new CalendarioPage(repos);
    adicionarSecao(QStringLiteral("📅  Calendário"), m_paginaCalendario);

    adicionarSecao(QStringLiteral("📈  Relatórios"), new RelatoriosPage(repos));

    // Navegação entre telas: aba "Anotações" da turma -> editor; prazo de tarefa no calendário -> tarefas.
    connect(m_paginaTurmas, &TurmasPage::abrirAnotacaoSolicitada, this, [this](int id) {
        m_paginaAnotacoes->selecionarAnotacao(id);
        irParaPagina(m_paginaAnotacoes);
    });
    connect(m_paginaCalendario, &CalendarioPage::abrirTarefaSolicitada, this, [this](int id) {
        m_paginaTarefas->selecionarTarefa(id);
        irParaPagina(m_paginaTarefas);
    });

    // Ctrl+1..9 e Ctrl+0 (10ª seção) navegam entre as seções.
    for (int i = 0; i < m_paginas->count() && i < 10; ++i) {
        auto *atalho = new QShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg((i + 1) % 10)), this);
        connect(atalho, &QShortcut::activated, this, [this, i] { irParaSecao(i); });
    }

    // Ctrl+K: busca global
    auto *atalhoBusca = new QShortcut(QKeySequence(QStringLiteral("Ctrl+K")), this);
    connect(atalhoBusca, &QShortcut::activated, this, &MainWindow::abrirBusca);

    // Backup automático: uma checagem logo após abrir (sem atrasar a abertura),
    // e depois uma por hora, para quem deixa o programa aberto por dias.
    QTimer::singleShot(3000, this, [this] { verificarBackupAutomatico(kIntervaloBackupHoras); });
    m_timerBackup = new QTimer(this);
    connect(m_timerBackup, &QTimer::timeout, this, [this] { verificarBackupAutomatico(kIntervaloBackupHoras); });
    m_timerBackup->start(60 * 60 * 1000);

    statusBar()->showMessage(QStringLiteral("Pronto · Ctrl+K busca em tudo"), 4000);

    // Reabre na última seção visitada (na primeira vez, abre o painel "Hoje").
    const int ultima = QSettings().value(QStringLiteral("ultimaSecao"), 0).toInt();
    irParaSecao(qBound(0, ultima, m_paginas->count() - 1));
}

void MainWindow::construirBarraLateral(QWidget *barra)
{
    auto *layout = new QVBoxLayout(barra);
    layout->setContentsMargins(0, 0, 0, 12);
    layout->setSpacing(0);

    auto *titulo = new QLabel(QStringLiteral("🎓 Professor\nOrganizado"));
    titulo->setObjectName(QStringLiteral("appTitle"));
    layout->addWidget(titulo);

    // Os botões de navegação ficam num layout próprio; adicionarSecao() os insere aqui.
    m_layoutNavegacao = new QVBoxLayout;
    m_layoutNavegacao->setContentsMargins(0, 4, 0, 0);
    m_layoutNavegacao->setSpacing(0);
    layout->addLayout(m_layoutNavegacao);

    layout->addStretch(1);

    // Ações fixas no rodapé: busca, backup, tema.
    auto *botaoBusca = new QPushButton(QStringLiteral("🔍  Buscar  (Ctrl+K)"));
    botaoBusca->setObjectName(QStringLiteral("footerButton"));
    botaoBusca->setCursor(Qt::PointingHandCursor);
    connect(botaoBusca, &QPushButton::clicked, this, &MainWindow::abrirBusca);
    layout->addWidget(botaoBusca);

    auto *botaoBackup = new QPushButton(QStringLiteral("💾  Backup"));
    botaoBackup->setObjectName(QStringLiteral("footerButton"));
    botaoBackup->setCursor(Qt::PointingHandCursor);
    connect(botaoBackup, &QPushButton::clicked, this, &MainWindow::abrirBackup);
    layout->addWidget(botaoBackup);

    m_botaoTema = new QPushButton;
    m_botaoTema->setObjectName(QStringLiteral("themeButton"));
    m_botaoTema->setCursor(Qt::PointingHandCursor);
    atualizarTextoBotaoTema();
    connect(m_botaoTema, &QPushButton::clicked, this, [this] {
        ThemeManager::alternar();
        atualizarTextoBotaoTema();
    });
    layout->addWidget(m_botaoTema);

    m_grupoNavegacao = new QButtonGroup(this);
    m_grupoNavegacao->setExclusive(true);
    connect(m_grupoNavegacao, &QButtonGroup::idClicked, this, &MainWindow::irParaSecao);
}

void MainWindow::adicionarSecao(const QString &titulo, QWidget *pagina)
{
    const int indice = m_paginas->addWidget(pagina);

    auto *botao = new QPushButton(titulo);
    botao->setCheckable(true);
    botao->setCursor(Qt::PointingHandCursor);
    m_grupoNavegacao->addButton(botao, indice);  // o id do botão = índice da página
    m_layoutNavegacao->addWidget(botao);
}

void MainWindow::irParaSecao(int indice)
{
    if (indice < 0 || indice >= m_paginas->count())
        return;
    m_paginas->setCurrentIndex(indice);
    if (auto *botao = m_grupoNavegacao->button(indice))
        botao->setChecked(true);
    QSettings().setValue(QStringLiteral("ultimaSecao"), indice);
}

void MainWindow::irParaPagina(QWidget *pagina)
{
    irParaSecao(m_paginas->indexOf(pagina));
}

void MainWindow::atualizarTextoBotaoTema()
{
    m_botaoTema->setText(ThemeManager::atual() == ThemeManager::Tema::Claro
                             ? QStringLiteral("🌙  Modo escuro")
                             : QStringLiteral("☀️  Modo claro"));
}

// ============================================================================
// Busca global
// ============================================================================

void MainWindow::abrirBusca()
{
    BuscaDialog dlg(m_repos.busca, this);
    if (dlg.exec() != QDialog::Accepted || !dlg.escolhido())
        return;
    navegarPara(*dlg.escolhido());
}

// Leva o usuário ao resultado escolhido na busca: abre a tela certa e seleciona o item.
void MainWindow::navegarPara(const ItemBusca &item)
{
    switch (item.tipo) {
    case TipoBusca::Turma:
        m_paginaTurmas->selecionarTurma(item.id);
        irParaPagina(m_paginaTurmas);
        break;
    case TipoBusca::Aluno:
        m_paginaTurmas->selecionarTurma(item.turmaId, item.id);
        irParaPagina(m_paginaTurmas);
        break;
    case TipoBusca::Anotacao:
        m_paginaAnotacoes->selecionarAnotacao(item.id);
        irParaPagina(m_paginaAnotacoes);
        break;
    case TipoBusca::Aula:
        m_paginaAulas->selecionarAula(item.id);
        irParaPagina(m_paginaAulas);
        break;
    case TipoBusca::Tarefa:
        m_paginaTarefas->selecionarTarefa(item.id);
        irParaPagina(m_paginaTarefas);
        break;
    case TipoBusca::Evento:
        m_paginaCalendario->irParaData(item.data.isValid() ? item.data : QDate::currentDate());
        irParaPagina(m_paginaCalendario);
        break;
    case TipoBusca::Anexo:
        abrirAnexo(this, item.caminho);  // abre o arquivo no programa padrão
        break;
    }
}

// ============================================================================
// Backup
// ============================================================================

void MainWindow::abrirBackup()
{
    BackupDialog dlg(this);
    dlg.exec();
}

void MainWindow::verificarBackupAutomatico(int intervaloHoras)
{
    QString erro;
    if (BackupService::backupAutomaticoSeNecessario(intervaloHoras, &erro))
        statusBar()->showMessage(QStringLiteral("Backup automático concluído."), 5000);
    else if (!erro.isEmpty())
        statusBar()->showMessage(QStringLiteral("Falha no backup automático: %1").arg(erro), 10000);
}

void MainWindow::closeEvent(QCloseEvent *evento)
{
    // Ao fechar, garante um backup recente (as telas com edição salvam em hideEvent/ao trocar).
    verificarBackupAutomatico(kIntervaloBackupAoFecharHoras);
    QMainWindow::closeEvent(evento);
}
