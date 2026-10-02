#include "ui/SobreDialog.h"

#include "core/BuildInfo.h"
#include "ui/ThemeManager.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QLabel *textoSimples(const QString &texto, const char *nome = nullptr)
{
    auto *rotulo = new QLabel(texto);
    rotulo->setTextFormat(Qt::PlainText);  // nada aqui vira HTML
    rotulo->setWordWrap(true);
    if (nome)
        rotulo->setObjectName(QString::fromLatin1(nome));
    return rotulo;
}

// Uma linha "rótulo  valor" do cartão de informações.
QWidget *linha(const QString &rotulo, const QString &valor)
{
    auto *w = new QWidget;
    w->setObjectName(QStringLiteral("linhaDoCartao"));  // fundo transparente dentro do cartão (ThemeManager)
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(12);
    auto *r = textoSimples(rotulo, "muted");
    r->setMinimumWidth(96);
    r->setMaximumWidth(96);
    auto *v = textoSimples(valor);
    v->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->addWidget(r, 0, Qt::AlignTop);
    l->addWidget(v, 1);
    return w;
}

}  // namespace

SobreDialog::SobreDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Sobre o Caderno+"));
    setMinimumWidth(480);

    auto *raiz = new QVBoxLayout(this);
    raiz->setContentsMargins(28, 24, 28, 20);
    raiz->setSpacing(16);

    // --- Cabeçalho: logo + nome + versão ---
    auto *cabecalho = new QHBoxLayout;
    cabecalho->setSpacing(16);
    auto *logo = new QLabel;
    const qreal escala = devicePixelRatioF();
    QPixmap marca(QStringLiteral(":/icons/icon-512.png"));
    marca = marca.scaled(QSize(72, 72) * escala, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    marca.setDevicePixelRatio(escala);
    logo->setPixmap(marca);
    logo->setFixedSize(72, 72);
    logo->setAccessibleName(QStringLiteral("Logo do Caderno+"));

    auto *nomes = new QVBoxLayout;
    nomes->setSpacing(2);
    auto *nome = new QLabel(QStringLiteral("Caderno<span style=\"color:%1\">+</span>")
                                .arg(ThemeManager::corHex(Tokens::Id::Accent)));
    nome->setObjectName(QStringLiteral("sobreNome"));
    nome->setTextFormat(Qt::RichText);  // texto fixo do programa, sem dados do usuário
    nomes->addStretch(1);
    nomes->addWidget(nome);
    nomes->addWidget(textoSimples(QStringLiteral("Versão %1").arg(identificacaoDoBuild()), "muted"));
    nomes->addStretch(1);
    cabecalho->addWidget(logo);
    cabecalho->addLayout(nomes, 1);
    raiz->addLayout(cabecalho);

    raiz->addWidget(textoSimples(
        QStringLiteral("Organizador para professores: turmas, notas, frequência, horário, aulas, anotações, tarefas e "
                       "calendário. Funciona sem internet e guarda tudo neste computador.")));

    // --- Informações ---
    auto *cartao = new QFrame;
    cartao->setObjectName(QStringLiteral("card"));
    auto *lc = new QVBoxLayout(cartao);
    lc->setContentsMargins(16, 14, 16, 14);
    lc->setSpacing(10);
    lc->addWidget(linha(QStringLiteral("Autor"), autorDoApp()));
    lc->addWidget(linha(QStringLiteral("Projeto"), enderecoDoProjeto()));
    lc->addWidget(linha(QStringLiteral("Licenças"),
                        QStringLiteral("Qt 6 (LGPLv3) · QXlsx (MIT) · Figtree (SIL OFL 1.1) · SQLite (domínio público)")));
    lc->addWidget(linha(QStringLiteral("Seus dados"),
                        QStringLiteral("Ficam só neste computador. O programa não envia nada pela internet, exceto o que você "
                                       "ligar em Configurações (Assistente de IA e aviso de versão nova).")));
    raiz->addWidget(cartao);

    // --- Botões ---
    auto *botoes = new QHBoxLayout;
    auto *abrir = new QPushButton(QStringLiteral("Abrir página do projeto"));
    abrir->setCursor(Qt::PointingHandCursor);
    connect(abrir, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl(enderecoDoProjeto())); });
    auto *fechar = new QPushButton(QStringLiteral("Fechar"));
    fechar->setObjectName(QStringLiteral("primary"));
    fechar->setDefault(true);
    connect(fechar, &QPushButton::clicked, this, &QDialog::accept);
    botoes->addWidget(abrir);
    botoes->addStretch(1);
    botoes->addWidget(fechar);
    raiz->addLayout(botoes);
}
