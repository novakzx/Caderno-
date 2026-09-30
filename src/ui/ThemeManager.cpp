#include "ui/ThemeManager.h"

#include "core/Contraste.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
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
}

// ============================================================================
// Folha de estilo (QSS)
// ============================================================================

QString ThemeManager::montarFolhaDeEstilo(Tema tema)
{
    // Modelo de QSS com marcadores @{nome-do-token}, trocados pelas cores do tema.
    // Raios: sm 6px (chips, badges) · md 10px (botões, campos, cartões) · lg 16px (painéis).
    // Tamanhos de texto: title 22px · heading 17px · body 15px (padrão) · small 13px · label 12px.
    QString qss = QStringLiteral(R"(
        /* Fonte no QSS também: o Windows impõe uma fonte própria às tabelas e cabeçalhos, que só o QSS sobrepõe. */
        QWidget { background: @{surface-100}; color: @{ink}; font-size: 15px; }
        QToolTip { background: @{surface-200}; color: @{ink}; border: 1px solid @{line}; border-radius: 6px; padding: 4px 8px; }

        /* Barra lateral: clara, com borda à direita; item ativo em primary-soft */
        #sidebar { background: @{surface-200}; border-right: 1px solid @{line}; }
        #sidebar QLabel#appTitle { background: transparent; color: @{ink};
            font-size: 22px; font-weight: 700; padding: 18px 16px 10px 16px; }
        #sidebar QPushButton { background: transparent; color: @{ink-muted};
            text-align: left; padding: 8px 14px; border: none; border-radius: 10px;
            margin: 1px 10px; }
        #sidebar QPushButton#footerButton { padding: 7px 14px; }
        #sidebar QPushButton:hover { background: @{surface-300}; color: @{ink}; }
        #sidebar QPushButton:checked { background: @{primary-soft}; color: @{primary}; font-weight: 600; }
        #sidebar QPushButton:checked:hover { background: @{primary-soft}; color: @{primary}; }
        #sidebar QPushButton:focus { border: 2px solid @{focus}; padding: 6px 12px; }
        #sidebar QPushButton#themeButton { border: 1px solid @{line}; }

        /* Páginas */
        QLabel#pageTitle { font-size: 22px; font-weight: 600; background: transparent; }
        QLabel#pageSubtitle, QLabel#muted { color: @{ink-muted}; font-size: 13px; background: transparent; }
        QLabel#sectionTitle { font-size: 17px; font-weight: 600; background: transparent; }
        QFrame#card { background: @{surface-200}; border: 1px solid @{line}; border-radius: 10px; }
        QFrame#card QLabel { background: transparent; }

        /* Mensagens de estado (sempre com texto; a cor só reforça) */
        QLabel[estado="sucesso"] { color: @{success}; font-weight: 600; background: transparent; }
        QLabel[estado="aviso"] { color: @{warning}; font-weight: 600; background: transparent; }
        QLabel[estado="erro"] { color: @{danger}; font-weight: 600; background: transparent; }

        /* Campos: borda de controle em line-strong; foco com anel de 2px em focus */
        QLineEdit, QSpinBox, QDoubleSpinBox, QDateEdit, QTimeEdit, QComboBox, QPlainTextEdit, QTextEdit {
            background: @{surface-200}; border: 1px solid @{line-strong}; border-radius: 10px;
            padding: 6px 8px; selection-background-color: @{primary}; selection-color: @{on-primary}; }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateEdit:focus, QTimeEdit:focus,
        QComboBox:focus, QPlainTextEdit:focus, QTextEdit:focus { border: 2px solid @{focus}; padding: 5px 7px; }
        QLineEdit:disabled, QSpinBox:disabled, QDateEdit:disabled, QComboBox:disabled { color: @{ink-muted}; background: @{surface-300}; }
        QComboBox QAbstractItemView { background: @{surface-200}; border: 1px solid @{line};
            selection-background-color: @{primary-soft}; selection-color: @{primary}; }

        /* Botões */
        QPushButton { background: @{surface-300}; border: 1px solid @{line}; border-radius: 10px; padding: 7px 14px; }
        QPushButton:hover { background: @{line}; }
        QPushButton:focus { border: 2px solid @{focus}; padding: 6px 13px; }
        QPushButton:disabled { color: @{ink-muted}; }
        QPushButton#primary { background: @{primary}; color: @{on-primary}; border: none; font-weight: 600; }
        QPushButton#primary:hover { background: @{primary}; }
        QPushButton#primary:focus { border: 2px solid @{ink}; padding: 5px 12px; }
        QPushButton#danger { color: @{danger}; }

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
            alternate-background-color: @{surface-300}; }
        QListWidget::item { padding: 6px 8px; }
        QListWidget::item:selected { background: @{primary-soft}; color: @{primary}; }
        QCalendarWidget QWidget { alternate-background-color: @{surface-300}; }
        QCalendarWidget QAbstractItemView { background: @{surface-200}; selection-background-color: @{primary};
            selection-color: @{on-primary}; }

        /* Selo "AGORA"/"PRÓXIMA" do painel Hoje */
        QLabel#badge, QFrame#card QLabel#badge { background: @{primary}; color: @{on-primary}; border-radius: 6px;
            padding: 3px 10px; font-size: 12px; font-weight: 700; }
        QCheckBox { background: transparent; spacing: 8px; }
        QScrollArea { background: transparent; border: none; }
        QScrollArea > QWidget > QWidget { background: transparent; }

        /* Divisores, rolagem e status */
        QSplitter::handle { background: transparent; }
        QStatusBar { background: @{surface-200}; color: @{ink-muted}; font-size: 13px; }
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

QIcon ThemeManager::icone(const QString &nome, int tamanho)
{
    QIcon icone;
    icone.addPixmap(desenharSvg(nome, cor(Tokens::Id::InkMuted), tamanho), QIcon::Normal, QIcon::Off);
    icone.addPixmap(desenharSvg(nome, cor(Tokens::Id::Primary), tamanho), QIcon::Normal, QIcon::On);
    return icone;
}

QIcon ThemeManager::iconeColorido(const QString &nome, Tokens::Id token, int tamanho)
{
    return QIcon(desenharSvg(nome, cor(token), tamanho));
}

QIcon ThemeManager::iconeColorido(const QString &nome, const QColor &cor, int tamanho)
{
    return QIcon(desenharSvg(nome, cor, tamanho));
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
