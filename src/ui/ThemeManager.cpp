#include "ui/ThemeManager.h"

#include <QApplication>
#include <QSettings>
#include <QStyleFactory>

namespace {

ThemeManager::Tema g_tema = ThemeManager::Tema::Claro;

// Paleta de cores: cada tema só precisa preencher estes valores.
struct Paleta {
    QString fundo, superficie, superficieAlt, borda, texto, textoSuave;
    QString destaque, destaqueTexto, hover, selecao, barraLateral, barraLateralTexto;
};

Paleta paletaClara()
{
    return {"#F4F6FA", "#FFFFFF", "#EEF1F7", "#DDE2EC", "#1F2937", "#6B7280",
            "#3B6FE0", "#FFFFFF", "#E6ECFA", "#D6E2FF", "#1E2A44", "#C9D3EA"};
}

Paleta paletaEscura()
{
    return {"#12161F", "#1B212D", "#232B3A", "#2E3748", "#E5E9F2", "#9AA4B8",
            "#5B8CFF", "#0B1020", "#27324A", "#2F4170", "#0D111A", "#B6C0D6"};
}

}  // namespace

QString ThemeManager::montarFolhaDeEstilo(Tema tema)
{
    const Paleta p = (tema == Tema::Escuro) ? paletaEscura() : paletaClara();

    // Modelo de QSS com marcadores @nome, trocados pelas cores da paleta.
    QString qss = QStringLiteral(R"(
        QWidget { background: @fundo; color: @texto; font-size: 10pt; }
        QToolTip { background: @superficie; color: @texto; border: 1px solid @borda; }

        /* Barra lateral */
        #sidebar { background: @barraLateral; }
        #sidebar QLabel#appTitle { background: transparent; color: white;
            font-size: 15pt; font-weight: 700; padding: 18px 16px 10px 16px; }
        #sidebar QPushButton { background: transparent; color: @barraLateralTexto;
            text-align: left; padding: 8px 16px; border: none; border-radius: 8px;
            margin: 1px 10px; font-size: 10.5pt; }
        #sidebar QPushButton#footerButton { font-size: 10pt; padding: 7px 16px; }
        #sidebar QPushButton:hover { background: rgba(255,255,255,0.08); }
        #sidebar QPushButton:checked { background: @destaque; color: @destaqueTexto; font-weight: 600; }
        #sidebar QPushButton#themeButton { border: 1px solid rgba(255,255,255,0.18); }

        /* Páginas */
        QLabel#pageTitle { font-size: 20pt; font-weight: 700; background: transparent; }
        QLabel#pageSubtitle, QLabel#muted { color: @textoSuave; background: transparent; }
        QLabel#sectionTitle { font-size: 12pt; font-weight: 600; background: transparent; }
        QFrame#card { background: @superficie; border: 1px solid @borda; border-radius: 10px; }
        QFrame#card QLabel { background: transparent; }

        /* Campos */
        QLineEdit, QSpinBox, QDateEdit, QComboBox, QPlainTextEdit, QTextEdit {
            background: @superficie; border: 1px solid @borda; border-radius: 6px;
            padding: 6px 8px; selection-background-color: @destaque; selection-color: @destaqueTexto; }
        QLineEdit:focus, QSpinBox:focus, QDateEdit:focus, QComboBox:focus,
        QPlainTextEdit:focus, QTextEdit:focus { border: 1px solid @destaque; }
        QComboBox QAbstractItemView { background: @superficie; selection-background-color: @selecao; }

        /* Botões */
        QPushButton { background: @superficieAlt; border: 1px solid @borda; border-radius: 6px; padding: 7px 14px; }
        QPushButton:hover { background: @hover; }
        QPushButton:disabled { color: @textoSuave; }
        QPushButton#primary { background: @destaque; color: @destaqueTexto; border: none; font-weight: 600; }
        QPushButton#primary:hover { background: @destaque; }
        QPushButton#danger { color: #D64545; }

        /* Tabelas */
        QTableWidget, QTableView { background: @superficie; alternate-background-color: @superficieAlt;
            border: 1px solid @borda; border-radius: 8px; gridline-color: @borda;
            selection-background-color: @selecao; selection-color: @texto; }
        QHeaderView::section { background: @superficieAlt; color: @textoSuave; border: none;
            border-bottom: 1px solid @borda; padding: 8px; font-weight: 600; }
        QTableCornerButton::section { background: @superficieAlt; border: none; }

        /* Abas (Turmas, Frequência) */
        QTabWidget::pane { border: 1px solid @borda; border-radius: 8px; top: -1px; background: @fundo; }
        QTabBar::tab { background: transparent; color: @textoSuave; padding: 8px 18px; margin-right: 2px;
            border: 1px solid transparent; border-top-left-radius: 8px; border-top-right-radius: 8px; }
        QTabBar::tab:selected { background: @superficie; color: @texto; border: 1px solid @borda; border-bottom-color: @superficie; font-weight: 600; }
        QTabBar::tab:hover:!selected { background: @hover; }
        QToolButton { background: @superficieAlt; border: 1px solid @borda; border-radius: 6px; }
        QToolButton:hover { background: @hover; }
        QToolButton:checked { background: @selecao; border: 1px solid @destaque; }
        QListWidget { background: @superficie; border: 1px solid @borda; border-radius: 8px;
            alternate-background-color: @superficieAlt; }
        QListWidget::item { padding: 6px 8px; }
        QListWidget::item:selected { background: @selecao; color: @texto; }
        QCalendarWidget QWidget { alternate-background-color: @superficieAlt; }
        QCalendarWidget QAbstractItemView { background: @superficie; selection-background-color: @destaque;
            selection-color: @destaqueTexto; }

        /* Selo "AGORA"/"PRÓXIMA" do painel Hoje */
        QLabel#badge { background: @destaque; color: @destaqueTexto; border-radius: 9px;
            padding: 3px 10px; font-size: 8.5pt; font-weight: 700; }
        QCheckBox { background: transparent; spacing: 8px; }
        QScrollArea { background: transparent; border: none; }
        QScrollArea > QWidget > QWidget { background: transparent; }

        /* Divisores, rolagem e status */
        QSplitter::handle { background: transparent; }
        QStatusBar { background: @superficie; color: @textoSuave; }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
        QScrollBar::handle:vertical { background: @borda; border-radius: 5px; min-height: 30px; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
        QScrollBar:horizontal { background: transparent; height: 10px; }
        QScrollBar::handle:horizontal { background: @borda; border-radius: 5px; min-width: 30px; }
        QDialog { background: @fundo; }
        QMessageBox { background: @fundo; }
    )");

    qss.replace("@fundo", p.fundo);
    qss.replace("@superficieAlt", p.superficieAlt);  // antes de @superficie (prefixo comum)
    qss.replace("@superficie", p.superficie);
    qss.replace("@borda", p.borda);
    qss.replace("@textoSuave", p.textoSuave);
    qss.replace("@texto", p.texto);
    qss.replace("@destaqueTexto", p.destaqueTexto);  // antes de @destaque
    qss.replace("@destaque", p.destaque);
    qss.replace("@hover", p.hover);
    qss.replace("@selecao", p.selecao);
    qss.replace("@barraLateralTexto", p.barraLateralTexto);  // antes de @barraLateral
    qss.replace("@barraLateral", p.barraLateral);
    return qss;
}

void ThemeManager::aplicar(Tema tema)
{
    g_tema = tema;
    qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));  // aparência igual em todos os SOs
    qApp->setStyleSheet(montarFolhaDeEstilo(tema));

    QSettings().setValue(QStringLiteral("tema"), tema == Tema::Escuro ? "escuro" : "claro");
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
