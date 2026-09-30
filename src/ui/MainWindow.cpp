#include "ui/MainWindow.h"

#include "core/BuildInfo.h"
#include "database/Repositorios.h"
#include "services/BackupService.h"
#include "ui/AnexosWidget.h"
#include "ui/AnotacoesPage.h"
#include "ui/AulasPage.h"
#include "ui/BackupDialog.h"
#include "ui/BarraDeTitulo.h"
#include "ui/BotoesAnimados.h"
#include "ui/BuscaDialog.h"
#include "ui/CalendarioPage.h"
#include "ui/FrequenciaPage.h"
#include "ui/HojePage.h"
#include "ui/HorarioPage.h"
#include "ui/NotasPage.h"
#include "ui/PilhaAnimada.h"
#include "ui/RelatoriosPage.h"
#include "ui/TarefasPage.h"
#include "ui/ThemeManager.h"
#include "ui/TurmasPage.h"

#include <QButtonGroup>
#include <QCloseEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMouseEvent>
#include <QSettings>
#include <QShortcut>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>
#include <utility>

namespace {
constexpr int kIntervaloBackupHoras = BackupService::kIntervaloPadraoHoras;  // backup automático: a cada 24 h
constexpr int kIntervaloBackupAoFecharHoras = 12;                            // e ao fechar, se o último tiver mais de 12 h
constexpr int kLarguraDaBorda = 6;                                           // faixa (px) em que o mouse redimensiona
}  // namespace

MainWindow::MainWindow(Repositorios &repos, const QString &nomeUsuario, const QString &emailUsuario, QWidget *parent)
    : QMainWindow(parent), m_repos(repos)
{
    setObjectName(QStringLiteral("janela"));
    setWindowFlag(Qt::FramelessWindowHint, true);  // sem a barra de título do sistema
    setWindowTitle(QStringLiteral("Caderno+"));    // aparece só na barra de tarefas
    resize(1280, 820);
    setMinimumSize(1000, 680);
    statusBar()->setSizeGripEnabled(false);

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
    construirBarraLateral(barra, nomeUsuario, emailUsuario);

    // --- Coluna da direita: barra de título + conteúdo ---
    auto *coluna = new QWidget;
    auto *layoutColuna = new QVBoxLayout(coluna);
    layoutColuna->setContentsMargins(0, 0, 0, 0);
    layoutColuna->setSpacing(0);
    m_barraTitulo = new BarraDeTitulo;
    m_barraTitulo->definirTexto(identificacaoDoBuild());
    m_paginas = new PilhaAnimada;
    layoutColuna->addWidget(m_barraTitulo);
    layoutColuna->addWidget(m_paginas, 1);

    layoutRaiz->addWidget(barra);
    layoutRaiz->addWidget(coluna, 1);
    setCentralWidget(central);

    // --- Seções (na ordem da barra lateral) ---
    adicionarSecao(QStringLiteral("hoje"), QStringLiteral("Hoje"), new HojePage(repos.agenda, repos.tarefas));

    m_paginaTurmas = new TurmasPage(repos);
    adicionarSecao(QStringLiteral("turmas"), QStringLiteral("Turmas"), m_paginaTurmas);

    adicionarSecao(QStringLiteral("notas"), QStringLiteral("Notas"),
                   new NotasPage(repos.turmas, repos.alunos, repos.avaliacoes, repos.notas));
    adicionarSecao(QStringLiteral("frequencia"), QStringLiteral("Frequência"), new FrequenciaPage(repos));
    adicionarSecao(QStringLiteral("horario"), QStringLiteral("Horário"), new HorarioPage(repos.horarios, repos.turmas));

    m_paginaAulas = new AulasPage(repos);
    adicionarSecao(QStringLiteral("aulas"), QStringLiteral("Aulas"), m_paginaAulas);

    m_paginaAnotacoes = new AnotacoesPage(repos);
    adicionarSecao(QStringLiteral("anotacoes"), QStringLiteral("Anotações"), m_paginaAnotacoes);

    m_paginaTarefas = new TarefasPage(repos);
    adicionarSecao(QStringLiteral("tarefas"), QStringLiteral("Tarefas"), m_paginaTarefas);

    m_paginaCalendario = new CalendarioPage(repos);
    adicionarSecao(QStringLiteral("calendario"), QStringLiteral("Calendário"), m_paginaCalendario);

    adicionarSecao(QStringLiteral("relatorios"), QStringLiteral("Relatórios"), new RelatoriosPage(repos));

    // Ícones e cores da barra lateral acompanham o tema claro/escuro.
    atualizarAparencia();
    connect(&ThemeManager::notificador(), &ThemeNotifier::temaMudou, this, &MainWindow::atualizarAparencia);

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

    // Como não há moldura, as bordas da janela são tratadas aqui (cursor e redimensionar).
    qApp->installEventFilter(this);

    // Reabre na última seção visitada (na primeira vez, abre o painel "Hoje").
    const int ultima = QSettings().value(QStringLiteral("ultimaSecao"), 0).toInt();
    irParaSecao(qBound(0, ultima, m_paginas->count() - 1));
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
    if (m_cursorDeBorda)
        QGuiApplication::restoreOverrideCursor();
}

BotaoNav *MainWindow::novoBotaoDoRodape(const QString &icone, const QString &texto)
{
    auto *botao = new BotaoNav(texto, icone);
    botao->setCheckable(false);
    m_botoesNav.append(botao);
    return botao;
}

void MainWindow::construirBarraLateral(QWidget *barra, const QString &nomeUsuario, const QString &emailUsuario)
{
    auto *layout = new QVBoxLayout(barra);
    layout->setContentsMargins(0, 0, 0, 12);
    layout->setSpacing(0);

    // Assinatura do design: nome em Figtree Bold, com o "+" em ocre (texto refeito quando o tema muda).
    // Também serve para arrastar a janela.
    m_titulo = new QLabel;
    m_titulo->setObjectName(QStringLiteral("appTitle"));
    m_titulo->setTextFormat(Qt::RichText);
    m_titulo->installEventFilter(this);
    layout->addWidget(m_titulo);

    // Os botões de navegação ficam num layout próprio; adicionarSecao() os insere aqui.
    m_layoutNavegacao = new QVBoxLayout;
    m_layoutNavegacao->setContentsMargins(0, 4, 0, 0);
    m_layoutNavegacao->setSpacing(0);
    layout->addLayout(m_layoutNavegacao);

    layout->addStretch(1);

    // Ações fixas no rodapé: busca, backup, tema.
    auto *botaoBusca = novoBotaoDoRodape(QStringLiteral("busca"), QStringLiteral("Buscar  (Ctrl+K)"));
    connect(botaoBusca, &QAbstractButton::clicked, this, &MainWindow::abrirBusca);
    layout->addWidget(botaoBusca);

    auto *botaoBackup = novoBotaoDoRodape(QStringLiteral("backup"), QStringLiteral("Backup"));
    connect(botaoBackup, &QAbstractButton::clicked, this, &MainWindow::abrirBackup);
    layout->addWidget(botaoBackup);

    m_botaoTema = novoBotaoDoRodape(QStringLiteral("lua"), QStringLiteral("Modo escuro"));
    connect(m_botaoTema, &QAbstractButton::clicked, this, [] { ThemeManager::alternar(); });
    layout->addWidget(m_botaoTema);

    // Conta conectada + Sair
    auto *divisor = new QFrame;
    divisor->setObjectName(QStringLiteral("divisor"));
    layout->addWidget(divisor);

    auto *linhaConta = new QHBoxLayout;
    linhaConta->setContentsMargins(16, 4, 12, 4);
    linhaConta->setSpacing(10);
    auto *avatar = new QLabel(nomeUsuario.trimmed().left(1).toUpper());
    avatar->setObjectName(QStringLiteral("avatar"));
    avatar->setAlignment(Qt::AlignCenter);
    auto *textos = new QVBoxLayout;
    textos->setSpacing(0);
    auto *nome = new QLabel(nomeUsuario);
    nome->setObjectName(QStringLiteral("nomeUsuario"));
    nome->setTextFormat(Qt::PlainText);
    nome->setToolTip(nomeUsuario);
    auto *email = new QLabel(emailUsuario);
    email->setObjectName(QStringLiteral("emailUsuario"));
    email->setTextFormat(Qt::PlainText);
    email->setToolTip(emailUsuario);
    nome->setMinimumWidth(10);
    email->setMinimumWidth(10);
    textos->addWidget(nome);
    textos->addWidget(email);
    linhaConta->addWidget(avatar);
    linhaConta->addLayout(textos, 1);
    layout->addLayout(linhaConta);

    auto *botaoSair = novoBotaoDoRodape(QStringLiteral("sair"), QStringLiteral("Sair da conta"));
    connect(botaoSair, &QAbstractButton::clicked, this, [this] {
        emit trocarContaSolicitado();
        close();
    });
    layout->addWidget(botaoSair);

    m_grupoNavegacao = new QButtonGroup(this);
    m_grupoNavegacao->setExclusive(true);
    connect(m_grupoNavegacao, &QButtonGroup::idClicked, this, &MainWindow::irParaSecao);
}

void MainWindow::adicionarSecao(const QString &icone, const QString &titulo, QWidget *pagina)
{
    const int indice = m_paginas->addWidget(pagina);

    auto *botao = new BotaoNav(titulo, icone);
    m_botoesNav.append(botao);
    m_grupoNavegacao->addButton(botao, indice);  // o id do botão = índice da página
    m_layoutNavegacao->addWidget(botao);
}

void MainWindow::irParaSecao(int indice)
{
    if (indice < 0 || indice >= m_paginas->count())
        return;
    m_paginas->irPara(indice);  // com fade
    if (auto *botao = m_grupoNavegacao->button(indice))
        botao->setChecked(true);
    QSettings().setValue(QStringLiteral("ultimaSecao"), indice);
}

void MainWindow::irParaPagina(QWidget *pagina)
{
    irParaSecao(m_paginas->indexOf(pagina));
}

// Refaz o que depende das cores do tema: ícones, assinatura e botão de tema.
// Roda na criação da janela e sempre que o tema muda.
void MainWindow::atualizarAparencia()
{
    m_titulo->setText(QStringLiteral("Caderno<span style=\"color:%1\">+</span>")
                          .arg(ThemeManager::corHex(Tokens::Id::Accent)));

    const bool claro = ThemeManager::atual() == ThemeManager::Tema::Claro;
    m_botaoTema->setText(claro ? QStringLiteral("Modo escuro") : QStringLiteral("Modo claro"));
    m_botaoTema->definirIcone(claro ? QStringLiteral("lua") : QStringLiteral("sol"));

    for (BotaoNav *botao : std::as_const(m_botoesNav))
        botao->recarregarIcones();
}

// ============================================================================
// Janela sem moldura: arrastar pelo título e redimensionar pelas bordas
// ============================================================================

Qt::Edges MainWindow::bordaEm(const QPoint &p) const
{
    Qt::Edges borda;
    if (isMaximized() || isFullScreen())
        return borda;
    const QRect r = frameGeometry();
    if (!r.adjusted(-2, -2, 2, 2).contains(p))
        return borda;
    if (p.x() <= r.left() + kLarguraDaBorda) borda |= Qt::LeftEdge;
    if (p.x() >= r.right() - kLarguraDaBorda) borda |= Qt::RightEdge;
    if (p.y() <= r.top() + kLarguraDaBorda) borda |= Qt::TopEdge;
    if (p.y() >= r.bottom() - kLarguraDaBorda) borda |= Qt::BottomEdge;
    return borda;
}

void MainWindow::atualizarCursorDaBorda(Qt::Edges borda)
{
    if (!borda) {
        if (m_cursorDeBorda)
            QGuiApplication::restoreOverrideCursor();
        m_cursorDeBorda = false;
        return;
    }
    Qt::CursorShape forma = Qt::SizeFDiagCursor;
    const bool horizontal = borda & (Qt::LeftEdge | Qt::RightEdge);
    const bool vertical = borda & (Qt::TopEdge | Qt::BottomEdge);
    if (horizontal && !vertical)
        forma = Qt::SizeHorCursor;
    else if (vertical && !horizontal)
        forma = Qt::SizeVerCursor;
    else if ((borda & Qt::LeftEdge && borda & Qt::BottomEdge) || (borda & Qt::RightEdge && borda & Qt::TopEdge))
        forma = Qt::SizeBDiagCursor;

    if (m_cursorDeBorda)
        QGuiApplication::changeOverrideCursor(QCursor(forma));
    else
        QGuiApplication::setOverrideCursor(QCursor(forma));
    m_cursorDeBorda = true;
}

bool MainWindow::eventFilter(QObject *objeto, QEvent *evento)
{
    // Clicar na assinatura "Caderno+" arrasta a janela (como a barra de título).
    if (objeto == m_titulo && evento->type() == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(evento);
        if (mouse->button() == Qt::LeftButton && windowHandle()) {
            windowHandle()->startSystemMove();
            return true;
        }
    }

    // Bordas: só para widgets desta janela (diálogos têm a própria moldura).
    const QEvent::Type tipo = evento->type();
    if ((tipo == QEvent::MouseMove || tipo == QEvent::MouseButtonPress) && objeto->isWidgetType()) {
        auto *widget = static_cast<QWidget *>(objeto);
        if (widget->window() == this) {
            auto *mouse = static_cast<QMouseEvent *>(evento);
            const Qt::Edges borda = bordaEm(mouse->globalPosition().toPoint());
            if (tipo == QEvent::MouseMove && !(mouse->buttons() & Qt::LeftButton))
                atualizarCursorDaBorda(borda);
            if (tipo == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton && borda && windowHandle()) {
                windowHandle()->startSystemResize(borda);
                return true;
            }
        }
    } else if (tipo == QEvent::Leave && objeto == this) {
        atualizarCursorDaBorda({});
    }
    return QMainWindow::eventFilter(objeto, evento);
}

void MainWindow::changeEvent(QEvent *evento)
{
    if (evento->type() == QEvent::WindowStateChange && m_barraTitulo) {
        const bool maximizada = isMaximized();
        m_barraTitulo->definirMaximizada(maximizada);
        setProperty("maximizada", maximizada);  // o QSS tira a borda quando maximizada
        style()->unpolish(this);
        style()->polish(this);
    }
    QMainWindow::changeEvent(evento);
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
