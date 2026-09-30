#include "ui/ThemeManager.h"

#include "core/Contraste.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QSettings>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleFactory>
#include <QSvgRenderer>
#include <QWidget>

namespace {

ThemeManager::Tema g_tema = ThemeManager::Tema::Claro;

bool ehEscuro(ThemeManager::Tema tema)
{
    return tema == ThemeManager::Tema::Escuro;
}

QColor corDoToken(Tokens::Id id, ThemeManager::Tema tema)
{
    return QColor(QLatin1String(Tokens::hex(id, ehEscuro(tema))));
}

// Ícones postos em botões (ThemeManager::iconeNoBotao): guardados para serem refeitos
// na cor do tema novo quando o tema muda.
struct IconeDeBotao {
    QPointer<QAbstractButton> botao;
    QString nome;
    Tokens::Id cor;
    int tamanho;
};
QList<IconeDeBotao> g_iconesDeBotoes;

}  // namespace

// ============================================================================
// Fontes
// ============================================================================

void ThemeManager::carregarFontes()
{
    // Figtree (OFL) embutida no .exe: Regular 400, SemiBold 600, Bold 700.
    // Se algum arquivo falhar, o programa segue com a fonte reserva (Segoe UI).
    const char *arquivos[] = {":/fonts/Figtree-Regular.ttf", ":/fonts/Figtree-SemiBold.ttf",
                              ":/fonts/Figtree-Bold.ttf"};
    for (const char *arquivo : arquivos)
        QFontDatabase::addApplicationFont(QLatin1String(arquivo));
}

void ThemeManager::aplicarFonte()
{
    // Escala "body" do design: 15px. A reserva é usada se a Figtree não estiver disponível.
    QFont fonte = qApp->font();
    fonte.setFamilies({QStringLiteral("Figtree"), QStringLiteral("Segoe UI")});
    fonte.setPixelSize(15);
    qApp->setFont(fonte);

    // O Windows dá fontes próprias a estas classes; sem isto elas ignorariam a Figtree.
    for (const char *classe : {"QAbstractItemView", "QHeaderView", "QMenu", "QStatusBar", "QToolTip", "QMessageBox"})
        qApp->setFont(fonte, classe);
}

// ============================================================================
// Imagens usadas pelo QSS (setas, marca de seleção)
// ============================================================================

// O QSS só aceita imagens como arquivo; então as setas são desenhadas na cor do tema e
// gravadas como PNG numa pasta de cache. Devolve a pasta (com "/" no final).
QString ThemeManager::prepararImagensDoQss(Tema tema)
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (base.isEmpty())
        base = QDir::tempPath() + QStringLiteral("/ProfOrganizer");
    const QString pasta = base + (ehEscuro(tema) ? QStringLiteral("/tema-escuro/") : QStringLiteral("/tema-claro/"));
    QDir().mkpath(pasta);

    struct Imagem {
        const char *nome;
        Tokens::Id cor;
    };
    static const Imagem imagens[] = {{"seta-baixo", Tokens::Id::InkMuted},
                                     {"seta-cima", Tokens::Id::InkMuted},
                                     {"seta-esquerda", Tokens::Id::Ink},
                                     {"seta-direita", Tokens::Id::Ink},
                                     {"marca", Tokens::Id::OnPrimary}};
    for (const Imagem &imagem : imagens)
        pixmap(QLatin1String(imagem.nome), corDoToken(imagem.cor, tema), 28)
            .save(pasta + QLatin1String(imagem.nome) + QStringLiteral(".png"), "PNG");
    return pasta;
}

// ============================================================================
// Folha de estilo (QSS)
// ============================================================================

QString ThemeManager::montarFolhaDeEstilo(Tema tema)
{
    // Modelo de QSS com marcadores @{nome-do-token}, trocados pelas cores do tema, e
    // @{pasta} (imagens das setas). Dividido em partes por causa do limite de tamanho
    // de texto literal do compilador.
    // Raios: sm 6px (chips, badges) · md 10px (botões, campos, cartões) · lg 16px (painéis).
    // Tamanhos de texto: title 22px · heading 17px · body 15px (padrão) · small 13px · label 12px.
    // Obs.: NÃO pôr font-family aqui (a lista de fontes no QSS trava o Qt); a família vem do
    // QApplication::setFont().
    QString qss = QStringLiteral(R"(
        QWidget { background: @{surface-100}; color: @{ink}; font-size: 15px; }
        QToolTip { background: @{surface-200}; color: @{ink}; border: 1px solid @{line}; border-radius: 6px; padding: 4px 8px; }

        /* Janela sem moldura do sistema */
        QMainWindow#janela { border: 1px solid @{line}; }
        QMainWindow#janela[maximizada="true"] { border: none; }
        #barraTitulo { background: @{surface-100}; }
        QLabel#versao { color: @{ink-muted}; font-size: 12px; background: transparent; }

        /* Barra lateral: clara, com borda à direita (os itens são desenhados por BotaoNav) */
        #sidebar { background: @{surface-200}; border-right: 1px solid @{line}; }
        #sidebar QLabel#appTitle { background: transparent; color: @{ink};
            font-size: 22px; font-weight: 700; padding: 18px 16px 10px 16px; }
        #sidebar QFrame#divisor { background: @{line}; max-height: 1px; min-height: 1px; margin: 6px 12px; border: none; }
        #sidebar QLabel { background: transparent; }
        #sidebar QLabel#avatar { background: @{primary-soft}; color: @{primary}; border-radius: 16px;
            font-weight: 700; min-width: 32px; max-width: 32px; min-height: 32px; max-height: 32px; }
        #sidebar QLabel#nomeUsuario { font-weight: 600; font-size: 14px; }
        #sidebar QLabel#emailUsuario { color: @{ink-muted}; font-size: 12px; }

        /* Páginas */
        QLabel#pageTitle { font-size: 22px; font-weight: 600; background: transparent; }
        QLabel#pageSubtitle, QLabel#muted { color: @{ink-muted}; font-size: 13px; background: transparent; }
        QLabel#sectionTitle { font-size: 17px; font-weight: 600; background: transparent; }
        QFrame#card { background: @{surface-200}; border: 1px solid @{line}; border-radius: 10px; }
        QFrame#card QLabel { background: transparent; }

        /* Tela de login */
        QDialog#login { background: @{surface-100}; border: 1px solid @{line}; }
        QFrame#painelMarca { background: @{primary}; border: none; }
        QFrame#painelMarca QLabel { background: transparent; color: @{on-primary}; }
        QLabel#marcaGrande { font-size: 34px; font-weight: 700; }
        QLabel#marcaFrase { font-size: 16px; }
        QLabel#tituloLogin { font-size: 26px; font-weight: 700; background: transparent; }
        QLabel#dicaLogin { color: @{ink-muted}; font-size: 13px; background: transparent; }
        QLabel#codigoRecuperacao { font-size: 22px; font-weight: 700; letter-spacing: 2px; background: @{surface-300};
            border: 1px dashed @{line-strong}; border-radius: 10px; padding: 14px; color: @{ink}; }

        /* Mensagens de estado (sempre com texto; a cor só reforça) */
        QLabel[estado="sucesso"] { color: @{success}; font-weight: 600; background: transparent; }
        QLabel[estado="aviso"] { color: @{warning}; font-weight: 600; background: transparent; }
        QLabel[estado="erro"] { color: @{danger}; font-weight: 600; background: transparent; }
    )");

    qss += QStringLiteral(R"(
        QLineEdit#tituloEditor { font-size: 20px; font-weight: 700; }
        QLineEdit#campoBusca { font-size: 17px; }

        /* Campos: borda de controle em line-strong; foco com anel de 2px em focus */
        QLineEdit, QPlainTextEdit, QTextEdit {
            background: @{surface-200}; border: 1px solid @{line-strong}; border-radius: 10px;
            padding: 6px 8px; selection-background-color: @{primary}; selection-color: @{on-primary}; }
        QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus { border: 2px solid @{focus}; padding: 5px 7px; }
        QComboBox, QSpinBox, QDoubleSpinBox, QDateEdit, QTimeEdit {
            background: @{surface-200}; border: 1px solid @{line-strong}; border-radius: 10px;
            padding: 6px 30px 6px 8px; selection-background-color: @{primary}; selection-color: @{on-primary}; }
        QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateEdit:focus, QTimeEdit:focus {
            border: 2px solid @{focus}; padding: 5px 29px 5px 7px; }
        QLineEdit:disabled, QSpinBox:disabled, QDateEdit:disabled, QComboBox:disabled {
            color: @{ink-muted}; background: @{surface-300}; }
        QComboBox QAbstractItemView { background: @{surface-200}; border: 1px solid @{line};
            selection-background-color: @{primary-soft}; selection-color: @{primary}; outline: none; }

        /* Setas dos campos (imagens na cor do tema) */
        QComboBox::drop-down, QDateEdit::drop-down, QTimeEdit::drop-down {
            subcontrol-origin: padding; subcontrol-position: center right; width: 28px; border: none; background: transparent; }
        QComboBox::down-arrow, QDateEdit::down-arrow, QTimeEdit::down-arrow {
            image: url("@{pasta}seta-baixo.png"); width: 14px; height: 14px; }
        QSpinBox::up-button, QDoubleSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right;
            width: 24px; border: none; background: transparent; margin: 3px 2px 0 0; }
        QSpinBox::down-button, QDoubleSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right;
            width: 24px; border: none; background: transparent; margin: 0 2px 3px 0; }
        QSpinBox::up-arrow, QDoubleSpinBox::up-arrow { image: url("@{pasta}seta-cima.png"); width: 11px; height: 11px; }
        QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { image: url("@{pasta}seta-baixo.png"); width: 11px; height: 11px; }

        /* Caixas de seleção */
        QCheckBox { background: transparent; spacing: 8px; }
        QCheckBox::indicator, QTableView::indicator, QListView::indicator { width: 18px; height: 18px;
            border: 1px solid @{line-strong}; border-radius: 6px; background: @{surface-200}; }
        QCheckBox::indicator:hover, QTableView::indicator:hover { border-color: @{primary}; }
        QCheckBox::indicator:checked, QTableView::indicator:checked, QListView::indicator:checked {
            background: @{primary}; border-color: @{primary}; image: url("@{pasta}marca.png"); }
        QCheckBox:focus { color: @{primary}; }

        /* Botões */
        QPushButton { background: @{surface-300}; border: 1px solid @{line}; border-radius: 10px; padding: 7px 14px; }
        QPushButton:hover { background: @{line}; }
        QPushButton:pressed { background: @{line-strong}; }
        QPushButton:focus { border: 2px solid @{focus}; padding: 6px 13px; }
        QPushButton:disabled { color: @{ink-muted}; background: @{surface-300}; }
        QPushButton#primary { background: @{primary}; color: @{on-primary}; border: none; font-weight: 600; }
        QPushButton#primary:hover { background: @{primary}; border: 2px solid @{primary-soft}; padding: 5px 12px; }
        QPushButton#primary:focus { border: 2px solid @{ink}; padding: 5px 12px; }
        QPushButton#primary:disabled { background: @{surface-300}; color: @{ink-muted}; }
        QPushButton#danger { color: @{danger}; }
        QPushButton#link { background: transparent; border: none; color: @{primary}; font-weight: 600; padding: 4px 6px; }
        QPushButton#link:hover { background: @{primary-soft}; }
    )");

    qss += QStringLiteral(R"(
        /* Tabelas */
        QTableWidget, QTableView { background: @{surface-200}; alternate-background-color: @{surface-300};
            border: 1px solid @{line}; border-radius: 10px; gridline-color: @{line};
            selection-background-color: @{primary-soft}; selection-color: @{ink}; }
        QHeaderView::section { background: @{surface-300}; color: @{ink-muted}; border: none;
            border-bottom: 1px solid @{line}; padding: 8px; font-weight: 600; }
        QTableCornerButton::section { background: @{surface-300}; border: none; }

        /* Abas (Turmas, Frequência) */
        QTabWidget::pane { border: 1px solid @{line}; border-radius: 10px; top: -1px; background: @{surface-100}; }
        QTabBar::tab { background: transparent; color: @{ink-muted}; padding: 8px 18px; margin-right: 2px;
            border: 1px solid transparent; border-top-left-radius: 10px; border-top-right-radius: 10px; }
        QTabBar::tab:selected { background: @{surface-200}; color: @{primary}; border: 1px solid @{line};
            border-bottom-color: @{surface-200}; font-weight: 600; }
        QTabBar::tab:hover:!selected { background: @{surface-300}; color: @{ink}; }
        QToolButton { background: @{surface-300}; border: 1px solid @{line}; border-radius: 6px; }
        QToolButton:hover { background: @{line}; }
        QToolButton:checked { background: @{primary-soft}; border: 1px solid @{primary}; }
        QListWidget { background: @{surface-200}; border: 1px solid @{line}; border-radius: 10px;
            alternate-background-color: @{surface-300}; outline: none; }
        QListWidget::item { padding: 6px 8px; }
        QListWidget::item:selected { background: @{primary-soft}; color: @{primary}; }

        /* Calendário */
        QCalendarWidget QWidget { alternate-background-color: @{surface-300}; }
        QCalendarWidget QAbstractItemView { background: @{surface-200}; selection-background-color: @{primary};
            selection-color: @{on-primary}; outline: none; }
        QCalendarWidget QWidget#qt_calendar_navigationbar { background: @{surface-200}; }
        QCalendarWidget QToolButton { background: transparent; border: none; border-radius: 8px;
            color: @{ink}; font-weight: 600; padding: 4px 10px; icon-size: 16px; }
        QCalendarWidget QToolButton:hover { background: @{surface-300}; }
        QCalendarWidget QToolButton#qt_calendar_prevmonth { qproperty-icon: url("@{pasta}seta-esquerda.png"); }
        QCalendarWidget QToolButton#qt_calendar_nextmonth { qproperty-icon: url("@{pasta}seta-direita.png"); }
        QCalendarWidget QToolButton::menu-indicator { image: none; }

        /* Selo "AGORA"/"PRÓXIMA" do painel Hoje (a 2ª regra vence a de "QFrame#card QLabel") */
        QLabel#badge, QFrame#card QLabel#badge { background: @{primary}; color: @{on-primary}; border-radius: 6px;
            padding: 3px 10px; font-size: 12px; font-weight: 700; }
        QScrollArea { background: transparent; border: none; }
        QScrollArea > QWidget > QWidget { background: transparent; }

        /* Divisores, rolagem e status */
        QSplitter::handle { background: transparent; }
        QStatusBar { background: @{surface-200}; color: @{ink-muted}; font-size: 13px; }
        QStatusBar::item { border: none; }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
        QScrollBar::handle:vertical { background: @{line-strong}; border-radius: 5px; min-height: 30px; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
        QScrollBar:horizontal { background: transparent; height: 10px; }
        QScrollBar::handle:horizontal { background: @{line-strong}; border-radius: 5px; min-width: 30px; }
        QDialog { background: @{surface-100}; }
        QMessageBox { background: @{surface-100}; }
        QMenu { background: @{surface-200}; border: 1px solid @{line}; border-radius: 10px; padding: 4px; }
        QMenu::item { padding: 6px 16px; border-radius: 6px; }
        QMenu::item:selected { background: @{primary-soft}; color: @{primary}; }
    )");

    for (std::size_t i = 0; i < Tokens::kTotal; ++i) {
        const auto id = static_cast<Tokens::Id>(i);
        qss.replace(QStringLiteral("@{%1}").arg(QLatin1String(Tokens::definicao(id).nome)),
                    QLatin1String(Tokens::hex(id, ehEscuro(tema))));
    }
    qss.replace(QStringLiteral("@{pasta}"), prepararImagensDoQss(tema));
    return qss;
}

// ============================================================================
// Tema
// ============================================================================

void ThemeManager::aplicar(Tema tema)
{
    g_tema = tema;
    qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));  // aparência igual em todos os SOs
    aplicarFonte();
    qApp->setStyleSheet(montarFolhaDeEstilo(tema));

    // Ícones postos em botões ficam na cor do tema novo.
    for (int i = g_iconesDeBotoes.size() - 1; i >= 0; --i) {
        const IconeDeBotao &item = g_iconesDeBotoes.at(i);
        if (item.botao.isNull())
            g_iconesDeBotoes.removeAt(i);
        else
            item.botao->setIcon(iconeColorido(item.nome, item.cor, item.tamanho));
    }

    QSettings().setValue(QStringLiteral("tema"), tema == Tema::Escuro ? "escuro" : "claro");
    emit notificador().temaMudou();
}

void ThemeManager::carregarSalvo()
{
    const QString salvo = QSettings().value(QStringLiteral("tema"), "claro").toString();
    aplicar(salvo == QLatin1String("escuro") ? Tema::Escuro : Tema::Claro);
}

ThemeManager::Tema ThemeManager::atual()
{
    return g_tema;
}

void ThemeManager::alternar()
{
    aplicar(g_tema == Tema::Claro ? Tema::Escuro : Tema::Claro);
}

ThemeNotifier &ThemeManager::notificador()
{
    static ThemeNotifier instancia;
    return instancia;
}

// ============================================================================
// Cores
// ============================================================================

QColor ThemeManager::cor(Tokens::Id id)
{
    return corDoToken(id, g_tema);
}

QColor ThemeManager::cor(Tokens::Id id, Tema tema)
{
    return corDoToken(id, tema);
}

QString ThemeManager::corHex(Tokens::Id id)
{
    return QString::fromLatin1(Tokens::hex(id, ehEscuro(g_tema)));
}

QColor ThemeManager::comAlfa(Tokens::Id id, int alfa)
{
    QColor c = cor(id);
    c.setAlpha(qBound(0, alfa, 255));
    return c;
}

QColor ThemeManager::corDaTurma(const QString &salva)
{
    const int numero = Tokens::numeroDaTurma(salva.toUtf8().constData());
    if (numero > 0)
        return cor(Tokens::turma(numero));
    const QColor manual(salva);  // turma antiga com cor escolhida à mão
    return manual.isValid() ? manual : cor(Tokens::Id::Turma6);
}

QString ThemeManager::corDaTurmaHex(const QString &salva)
{
    return corDaTurma(salva).name();
}

QColor ThemeManager::textoSobreTurma(const QString &salva)
{
    if (Tokens::numeroDaTurma(salva.toUtf8().constData()) > 0)
        return cor(Tokens::Id::OnTurma);
    // Cor manual: escolhe, entre o texto claro e o escuro do tema, o de melhor contraste.
    const QColor fundo = corDaTurma(salva);
    const QColor claro = cor(Tokens::Id::OnTurma, Tema::Claro);
    const QColor escuro = cor(Tokens::Id::Ink, Tema::Claro);
    const QByteArray f = fundo.name().toLatin1();
    return Contraste::razao(claro.name().toLatin1().constData(), f.constData())
                   >= Contraste::razao(escuro.name().toLatin1().constData(), f.constData())
               ? claro
               : escuro;
}

QColor ThemeManager::corDaSituacao(const QString &situacao)
{
    // P presente · F falta · J justificada · A atraso (conta como presença). A letra sempre aparece.
    if (situacao == QLatin1String("P"))
        return cor(Tokens::Id::Success);
    if (situacao == QLatin1String("F"))
        return cor(Tokens::Id::Danger);
    if (situacao == QLatin1String("J"))
        return cor(Tokens::Id::Warning);
    if (situacao == QLatin1String("A"))
        return cor(Tokens::Id::Ink);  // o ocre (accent) não tem contraste de texto no tema claro
    return QColor();  // sem marca: nada a destacar
}

QColor ThemeManager::fundoDaSituacao(const QString &situacao)
{
    // Só o atraso ganha um fundo (ocre suave); P/F/J são reforçadas apenas pela cor da letra.
    if (situacao == QLatin1String("A"))
        return cor(Tokens::Id::AccentSoft);
    return QColor();
}

// ============================================================================
// Ícones
// ============================================================================

namespace {

// Lê o SVG do recurso e troca "currentColor" pela cor pedida; desenha num pixmap nítido.
QPixmap desenharSvg(const QString &nome, const QColor &cor, int tamanho)
{
    QFile arquivo(QStringLiteral(":/icons/%1.svg").arg(nome));
    if (!arquivo.open(QIODevice::ReadOnly))
        return QPixmap();
    QByteArray svg = arquivo.readAll();
    svg.replace("currentColor", cor.name().toLatin1());

    QSvgRenderer renderizador(svg);
    const qreal escala = qApp->devicePixelRatio() > 1.0 ? 2.0 : 1.0;
    QPixmap pixmap(QSize(tamanho, tamanho) * escala);
    pixmap.fill(Qt::transparent);
    QPainter pintor(&pixmap);
    renderizador.render(&pintor);
    pintor.end();
    pixmap.setDevicePixelRatio(escala);
    return pixmap;
}

}  // namespace

QPixmap ThemeManager::pixmap(const QString &nome, const QColor &cor, int tamanho)
{
    return desenharSvg(nome, cor, tamanho);
}

QIcon ThemeManager::icone(const QString &nome, int tamanho)
{
    QIcon icone;
    icone.addPixmap(desenharSvg(nome, cor(Tokens::Id::InkMuted), tamanho), QIcon::Normal, QIcon::Off);
    icone.addPixmap(desenharSvg(nome, cor(Tokens::Id::Primary), tamanho), QIcon::Normal, QIcon::On);
    return icone;
}

QIcon ThemeManager::iconeColorido(const QString &nome, Tokens::Id token, int tamanho)
{
    return iconeColorido(nome, cor(token), tamanho);
}

QIcon ThemeManager::iconeColorido(const QString &nome, const QColor &cor, int tamanho)
{
    QIcon icone(desenharSvg(nome, cor, tamanho));
    // Botão desativado: o mesmo ícone, em cinza.
    icone.addPixmap(desenharSvg(nome, ThemeManager::cor(Tokens::Id::InkMuted), tamanho), QIcon::Disabled);
    return icone;
}

void ThemeManager::iconeNoBotao(QAbstractButton *botao, const QString &nome, Tokens::Id cor, int tamanho)
{
    if (!botao)
        return;
    botao->setIconSize(QSize(tamanho, tamanho));
    botao->setIcon(iconeColorido(nome, cor, tamanho));
    g_iconesDeBotoes.append({botao, nome, cor, tamanho});
}

void ThemeManager::definirEstado(QWidget *widget, Estado estado)
{
    const char *valor = "";
    switch (estado) {
    case Estado::Sucesso: valor = "sucesso"; break;
    case Estado::Aviso:   valor = "aviso"; break;
    case Estado::Erro:    valor = "erro"; break;
    case Estado::Neutro:  break;
    }
    widget->setProperty("estado", QString::fromLatin1(valor));
    // O QSS por propriedade só é reavaliado depois de "repolir" o widget.
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}
