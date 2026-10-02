#include "ui/LoginDialog.h"

#include "core/BuildInfo.h"
#include "core/ContaUtil.h"
#include "ui/BotoesAnimados.h"
#include "ui/IconeDaJanela.h"
#include "ui/PilhaAnimada.h"
#include "ui/ThemeManager.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QSettings>
#include <QSvgRenderer>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

namespace {

std::string utf8(const QString &s)
{
    const QByteArray b = s.toUtf8();
    return std::string(b.constData(), static_cast<std::size_t>(b.size()));
}

}  // namespace

// ============================================================================
// Indicador de força da senha: 4 segmentos (o texto ao lado diz o nível: a cor só reforça)
// ============================================================================

class BarraDeForca : public QWidget {
public:
    explicit BarraDeForca(QWidget *pai = nullptr) : QWidget(pai) { setFixedHeight(6); }
    void setNivel(int nivel)
    {
        m_nivel = nivel;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int espaco = 4;
        const qreal largura = (width() - 3 * espaco) / 4.0;
        static const Tokens::Id cores[] = {Tokens::Id::Danger, Tokens::Id::Warning, Tokens::Id::Primary, Tokens::Id::Success};
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 4; ++i) {
            p.setBrush(i < m_nivel ? ThemeManager::cor(cores[qBound(0, m_nivel - 1, 3)]) : ThemeManager::cor(Tokens::Id::Line));
            p.drawRoundedRect(QRectF(i * (largura + espaco), 0, largura, height()), 3, 3);
        }
    }

private:
    int m_nivel = 0;
};

// ============================================================================
// Montagem
// ============================================================================

LoginDialog::LoginDialog(ContaService &contas, bool haDadosAntigos, QWidget *parent)
    : QDialog(parent), m_contas(contas), m_haDadosAntigos(haDadosAntigos)
{
    setObjectName(QStringLiteral("login"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setWindowTitle(QStringLiteral("Caderno+"));
    setFixedSize(920, 620);

    auto *raiz = new QHBoxLayout(this);
    raiz->setContentsMargins(1, 1, 1, 1);  // 1px para a borda do QSS
    raiz->setSpacing(0);
    raiz->addWidget(criarPainelMarca());

    // Lado direito: botões da janela no topo + formulários
    auto *direita = new QVBoxLayout;
    direita->setContentsMargins(0, 0, 0, 0);
    direita->setSpacing(0);

    auto *topo = new QHBoxLayout;
    topo->setContentsMargins(0, 0, 0, 0);
    topo->setSpacing(0);
    topo->addStretch(1);
    auto *botaoTema = new BotaoJanela(QStringLiteral("lua"), false);
    botaoTema->setToolTip(QStringLiteral("Alternar tema claro/escuro"));
    connect(botaoTema, &QAbstractButton::clicked, this, [] { ThemeManager::alternar(); });
    connect(&ThemeManager::notificador(), &ThemeNotifier::temaMudou, this, [this, botaoTema] {
        botaoTema->definirIcone(ThemeManager::atual() == ThemeManager::Tema::Claro ? QStringLiteral("lua") : QStringLiteral("sol"));
        atualizarIcones();
    });
    botaoTema->definirIcone(ThemeManager::atual() == ThemeManager::Tema::Claro ? QStringLiteral("lua") : QStringLiteral("sol"));
    auto *fechar = new BotaoJanela(QStringLiteral("janela-fechar"), true);
    fechar->setToolTip(QStringLiteral("Fechar"));
    connect(fechar, &QAbstractButton::clicked, this, &QDialog::reject);
    topo->addWidget(botaoTema);
    topo->addWidget(fechar);
    direita->addLayout(topo);

    m_paginas = new PilhaAnimada;
    m_paginas->addWidget(criarFormEntrar());
    m_paginas->addWidget(criarFormCriar());
    m_paginas->addWidget(criarFormRecuperar());
    direita->addWidget(m_paginas, 1);
    raiz->addLayout(direita, 1);

    atualizarIcones();
    irPara(m_contas.temContas() ? PaginaEntrar : PaginaCriar);

    // Se o cadastro de contas não abriu, mostra o motivo (não dá para entrar sem ele).
    if (!m_contas.disponivel()) {
        const QString motivo = QStringLiteral("Não foi possível abrir o cadastro de contas: %1").arg(m_contas.erroDeAbertura());
        mostrarErro(m_entrarErro, motivo);
        mostrarErro(m_criarErro, motivo);
    }
}

QWidget *LoginDialog::criarPainelMarca()
{
    auto *painel = new QFrame;
    painel->setObjectName(QStringLiteral("painelMarca"));
    painel->setFixedWidth(340);
    auto *layout = new QVBoxLayout(painel);
    layout->setContentsMargins(36, 44, 32, 36);
    layout->setSpacing(10);

    // Símbolo (página de caderno com o "+") desenhado do SVG da marca.
    auto *simbolo = new QLabel;
    QSvgRenderer renderizador(QStringLiteral(":/icons/caderno-mark.svg"));
    const qreal escala = qApp->devicePixelRatio() > 1.0 ? 2.0 : 1.0;
    QPixmap pixmap(QSize(64, 64) * escala);
    pixmap.fill(Qt::transparent);
    {
        QPainter pintor(&pixmap);
        renderizador.render(&pintor);
    }
    pixmap.setDevicePixelRatio(escala);
    simbolo->setPixmap(pixmap);
    layout->addWidget(simbolo);

    auto *nome = new QLabel(QStringLiteral("Caderno+"));
    nome->setObjectName(QStringLiteral("marcaGrande"));
    layout->addWidget(nome);

    auto *frase = new QLabel(QStringLiteral("Suas turmas, notas e aulas num só lugar."));
    frase->setObjectName(QStringLiteral("marcaFrase"));
    frase->setWordWrap(true);
    layout->addWidget(frase);
    layout->addSpacing(26);

    const char *itens[][2] = {{"frequencia", "Chamada, notas e frequência sem planilhas soltas"},
                              {"calendario", "Horário, aulas e provas sempre à mão"},
                              {"cadeado", "Funciona sem internet: os dados ficam no seu computador"}};
    for (const auto &item : itens) {
        auto *linha = new QHBoxLayout;
        linha->setSpacing(12);
        auto *icone = new QLabel;
        icone->setPixmap(ThemeManager::pixmap(QLatin1String(item[0]), ThemeManager::cor(Tokens::Id::OnPrimary), 22));
        icone->setFixedSize(24, 24);
        icone->setProperty("iconeDaMarca", QLatin1String(item[0]));
        auto *texto = new QLabel(QString::fromUtf8(item[1]));
        texto->setWordWrap(true);
        linha->addWidget(icone, 0, Qt::AlignTop);
        linha->addWidget(texto, 1);
        layout->addLayout(linha);
    }
    layout->addStretch(1);

    auto *rodape = new QLabel(QStringLiteral("Versão %1  ·  por %2").arg(versaoDoApp(), autorDoApp()));
    rodape->setObjectName(QStringLiteral("marcaFrase"));
    rodape->setTextFormat(Qt::PlainText);
    rodape->setWordWrap(true);
    layout->addWidget(rodape);
    return painel;
}

QLineEdit *LoginDialog::novoCampo(const QString &dica, const QString &icone, bool senha, QWidget *pai)
{
    auto *campo = new QLineEdit(pai);
    campo->setPlaceholderText(dica);
    campo->setMinimumHeight(42);
    campo->setMaxLength(senha ? 128 : 254);

    auto *inicial = campo->addAction(ThemeManager::iconeColorido(icone, Tokens::Id::InkMuted, 18), QLineEdit::LeadingPosition);
    m_iconesDeCampos.append({inicial, icone});

    if (senha) {
        campo->setEchoMode(QLineEdit::Password);
        auto *olho = campo->addAction(ThemeManager::iconeColorido(QStringLiteral("olho"), Tokens::Id::InkMuted, 18),
                                      QLineEdit::TrailingPosition);
        olho->setToolTip(QStringLiteral("Mostrar ou ocultar a senha"));
        connect(olho, &QAction::triggered, this, [this, campo, olho] {
            const bool mostrar = campo->echoMode() == QLineEdit::Password;
            campo->setEchoMode(mostrar ? QLineEdit::Normal : QLineEdit::Password);
            olho->setIcon(ThemeManager::iconeColorido(mostrar ? QStringLiteral("olho-fechado") : QStringLiteral("olho"),
                                                      Tokens::Id::InkMuted, 18));
        });
        m_olhos.append(olho);
    }
    return campo;
}

namespace {

// Cabeçalho padrão dos formulários: título grande + frase de apoio.
void cabecalho(QVBoxLayout *layout, const QString &titulo, const QString &dica)
{
    auto *t = new QLabel(titulo);
    t->setObjectName(QStringLiteral("tituloLogin"));
    auto *d = new QLabel(dica);
    d->setObjectName(QStringLiteral("dicaLogin"));
    d->setWordWrap(true);
    layout->addWidget(t);
    layout->addWidget(d);
    layout->addSpacing(14);
}

QLabel *novoRotuloDeErro()
{
    auto *l = new QLabel;
    l->setWordWrap(true);
    l->setVisible(false);
    return l;
}

QPushButton *botaoPrimario(const QString &texto)
{
    auto *b = new QPushButton(texto);
    b->setObjectName(QStringLiteral("primary"));
    b->setMinimumHeight(44);
    b->setDefault(true);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QPushButton *botaoLink(const QString &texto)
{
    auto *b = new QPushButton(texto);
    b->setObjectName(QStringLiteral("link"));
    b->setCursor(Qt::PointingHandCursor);
    b->setAutoDefault(false);
    return b;
}

}  // namespace

void LoginDialog::definirAviso(const QString &texto)
{
    m_entrarErro->setText(texto);
    ThemeManager::definirEstado(m_entrarErro, ThemeManager::Estado::Aviso);
    m_entrarErro->setVisible(true);
}

QWidget *LoginDialog::criarFormEntrar()
{
    auto *pagina = new QWidget;
    auto *layout = new QVBoxLayout(pagina);
    layout->setContentsMargins(56, 8, 56, 40);
    layout->setSpacing(10);
    layout->addStretch(1);
    cabecalho(layout, QStringLiteral("Bem-vindo de volta"), QStringLiteral("Entre com a sua conta para abrir o Caderno+."));

    m_entrarEmail = novoCampo(QStringLiteral("E-mail"), QStringLiteral("email"), false, pagina);
    m_entrarEmail->setText(QSettings().value(QStringLiteral("ultimoEmail")).toString());
    m_entrarSenha = novoCampo(QStringLiteral("Senha"), QStringLiteral("cadeado"), true, pagina);
    m_entrarErro = novoRotuloDeErro();
    m_botaoEntrar = botaoPrimario(QStringLiteral("Entrar"));
    connect(m_botaoEntrar, &QPushButton::clicked, this, &LoginDialog::acaoEntrar);

    auto *esqueci = botaoLink(QStringLiteral("Esqueci minha senha"));
    connect(esqueci, &QPushButton::clicked, this, [this] {
        m_recEmail->setText(m_entrarEmail->text());
        irPara(PaginaRecuperar);
    });
    auto *criar = botaoLink(QStringLiteral("Criar conta"));
    connect(criar, &QPushButton::clicked, this, [this] { irPara(PaginaCriar); });

    layout->addWidget(m_entrarEmail);
    layout->addWidget(m_entrarSenha);
    layout->addWidget(m_entrarErro);
    layout->addSpacing(4);
    layout->addWidget(m_botaoEntrar);
    auto *links = new QHBoxLayout;
    links->addWidget(esqueci);
    links->addStretch(1);
    links->addWidget(criar);
    layout->addLayout(links);
    layout->addStretch(2);
    return pagina;
}

QWidget *LoginDialog::criarFormCriar()
{
    auto *pagina = new QWidget;
    auto *layout = new QVBoxLayout(pagina);
    layout->setContentsMargins(56, 8, 56, 32);
    layout->setSpacing(10);
    layout->addStretch(1);
    cabecalho(layout, QStringLiteral("Criar conta"),
              m_haDadosAntigos ? QStringLiteral("Os dados que você já cadastrou neste computador ficarão guardados nesta conta.")
                               : QStringLiteral("Cada conta tem os próprios dados. Leva menos de um minuto."));

    m_criarNome = novoCampo(QStringLiteral("Seu nome"), QStringLiteral("usuario"), false, pagina);
    m_criarNome->setMaxLength(80);
    m_criarEmail = novoCampo(QStringLiteral("E-mail"), QStringLiteral("email"), false, pagina);
    m_criarSenha = novoCampo(QStringLiteral("Senha (mínimo de 8 caracteres)"), QStringLiteral("cadeado"), true, pagina);
    m_criarConfirma = novoCampo(QStringLiteral("Repita a senha"), QStringLiteral("cadeado"), true, pagina);

    m_forca = new BarraDeForca;
    m_textoForca = new QLabel;
    m_textoForca->setObjectName(QStringLiteral("dicaLogin"));
    connect(m_criarSenha, &QLineEdit::textChanged, this, [this](const QString &senha) {
        const int nivel = ContaUtil::forcaDaSenha(utf8(senha));
        static const char *nomes[] = {"", "Senha fraca", "Senha razoável", "Senha boa", "Senha forte"};
        m_forca->setNivel(nivel);
        m_textoForca->setText(senha.isEmpty() ? QString() : QString::fromUtf8(nomes[qBound(0, nivel, 4)]));
    });

    m_criarErro = novoRotuloDeErro();
    m_botaoCriar = botaoPrimario(QStringLiteral("Criar conta"));
    connect(m_botaoCriar, &QPushButton::clicked, this, &LoginDialog::acaoCriar);
    auto *jaTenho = botaoLink(QStringLiteral("Já tenho conta"));
    connect(jaTenho, &QPushButton::clicked, this, [this] { irPara(PaginaEntrar); });

    layout->addWidget(m_criarNome);
    layout->addWidget(m_criarEmail);
    layout->addWidget(m_criarSenha);
    layout->addWidget(m_forca);
    layout->addWidget(m_textoForca);
    layout->addWidget(m_criarConfirma);
    layout->addWidget(m_criarErro);
    layout->addSpacing(4);
    layout->addWidget(m_botaoCriar);
    layout->addWidget(jaTenho, 0, Qt::AlignLeft);
    layout->addStretch(2);
    return pagina;
}

QWidget *LoginDialog::criarFormRecuperar()
{
    auto *pagina = new QWidget;
    auto *layout = new QVBoxLayout(pagina);
    layout->setContentsMargins(56, 8, 56, 32);
    layout->setSpacing(10);
    layout->addStretch(1);
    cabecalho(layout, QStringLiteral("Recuperar senha"),
              QStringLiteral("Use o código de recuperação que apareceu quando você criou a conta."));

    m_recEmail = novoCampo(QStringLiteral("E-mail"), QStringLiteral("email"), false, pagina);
    m_recCodigo = novoCampo(QStringLiteral("Código de recuperação (XXXX-XXXX-...)"), QStringLiteral("chave"), false, pagina);
    m_recCodigo->setMaxLength(40);
    m_recSenha = novoCampo(QStringLiteral("Nova senha"), QStringLiteral("cadeado"), true, pagina);
    m_recConfirma = novoCampo(QStringLiteral("Repita a nova senha"), QStringLiteral("cadeado"), true, pagina);
    m_recErro = novoRotuloDeErro();
    m_botaoRecuperar = botaoPrimario(QStringLiteral("Redefinir senha"));
    connect(m_botaoRecuperar, &QPushButton::clicked, this, &LoginDialog::acaoRedefinir);
    auto *voltar = botaoLink(QStringLiteral("Voltar para entrar"));
    connect(voltar, &QPushButton::clicked, this, [this] { irPara(PaginaEntrar); });

    layout->addWidget(m_recEmail);
    layout->addWidget(m_recCodigo);
    layout->addWidget(m_recSenha);
    layout->addWidget(m_recConfirma);
    layout->addWidget(m_recErro);
    layout->addSpacing(4);
    layout->addWidget(m_botaoRecuperar);
    layout->addWidget(voltar, 0, Qt::AlignLeft);
    layout->addStretch(2);
    return pagina;
}

// ============================================================================
// Comportamento
// ============================================================================

void LoginDialog::irPara(Pagina pagina)
{
    // Limpa os erros e as senhas ao trocar de formulário.
    for (QLabel *erro : {m_entrarErro, m_criarErro, m_recErro})
        erro->setVisible(false);
    for (QLineEdit *campo : {m_entrarSenha, m_criarSenha, m_criarConfirma, m_recSenha, m_recConfirma})
        campo->clear();
    m_paginas->irPara(pagina);

    QLineEdit *foco = pagina == PaginaEntrar ? (m_entrarEmail->text().isEmpty() ? m_entrarEmail : m_entrarSenha)
                      : pagina == PaginaCriar ? m_criarNome
                                              : m_recEmail;
    QTimer::singleShot(0, foco, qOverload<>(&QWidget::setFocus));
}

void LoginDialog::mostrarErro(QLabel *rotulo, const QString &texto)
{
    rotulo->setText(texto);
    rotulo->setVisible(!texto.isEmpty());
    ThemeManager::definirEstado(rotulo, ThemeManager::Estado::Erro);
}

// Pequena "tremida" horizontal do formulário quando algo dá errado.
void LoginDialog::tremer()
{
    QWidget *alvo = m_paginas->currentWidget();
    if (!alvo)
        return;
    const QPoint origem = alvo->pos();
    auto *animacao = new QPropertyAnimation(alvo, "pos", alvo);
    animacao->setDuration(320);
    animacao->setKeyValueAt(0.0, origem);
    animacao->setKeyValueAt(0.15, origem + QPoint(-10, 0));
    animacao->setKeyValueAt(0.35, origem + QPoint(10, 0));
    animacao->setKeyValueAt(0.55, origem + QPoint(-6, 0));
    animacao->setKeyValueAt(0.75, origem + QPoint(6, 0));
    animacao->setKeyValueAt(1.0, origem);
    animacao->start(QAbstractAnimation::DeleteWhenStopped);
}

namespace {

// Enquanto a senha é conferida (leva uma fração de segundo de propósito), mostra "aguarde".
struct Ocupado {
    explicit Ocupado(QPushButton *botao, const QString &texto) : m_botao(botao), m_original(botao->text())
    {
        m_botao->setEnabled(false);
        m_botao->setText(texto);
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QApplication::processEvents();
    }
    ~Ocupado()
    {
        QApplication::restoreOverrideCursor();
        m_botao->setText(m_original);
        m_botao->setEnabled(true);
    }
    QPushButton *m_botao;
    QString m_original;
};

}  // namespace

void LoginDialog::acaoEntrar()
{
    const QString email = m_entrarEmail->text().trimmed();
    const QString senha = m_entrarSenha->text();
    if (email.isEmpty() || senha.isEmpty()) {
        mostrarErro(m_entrarErro, QStringLiteral("Informe o e-mail e a senha."));
        tremer();
        return;
    }
    ResultadoConta r;
    {
        Ocupado ocupado(m_botaoEntrar, QStringLiteral("Entrando…"));
        r = m_contas.entrar(email, senha);
    }
    if (!r.ok) {
        mostrarErro(m_entrarErro, r.erro);
        m_entrarSenha->clear();
        m_entrarSenha->setFocus();
        tremer();
        return;
    }
    QSettings().setValue(QStringLiteral("ultimoEmail"), r.conta.email);
    concluir(r.conta);
}

void LoginDialog::acaoCriar()
{
    ResultadoConta r;
    {
        Ocupado ocupado(m_botaoCriar, QStringLiteral("Criando…"));
        r = m_contas.registrar(m_criarNome->text(), m_criarEmail->text(), m_criarSenha->text(), m_criarConfirma->text());
    }
    if (!r.ok) {
        mostrarErro(m_criarErro, r.erro);
        tremer();
        return;
    }
    QSettings().setValue(QStringLiteral("ultimoEmail"), r.conta.email);
    mostrarCodigoDeRecuperacao(r.codigoRecuperacao, QStringLiteral("Conta criada!"));
    concluir(r.conta);
}

void LoginDialog::acaoRedefinir()
{
    if (m_recSenha->text() != m_recConfirma->text()) {
        mostrarErro(m_recErro, QStringLiteral("A confirmação da senha não confere."));
        tremer();
        return;
    }
    ResultadoConta r;
    {
        Ocupado ocupado(m_botaoRecuperar, QStringLiteral("Redefinindo…"));
        r = m_contas.redefinirSenha(m_recEmail->text(), m_recCodigo->text(), m_recSenha->text());
    }
    if (!r.ok) {
        mostrarErro(m_recErro, r.erro);
        tremer();
        return;
    }
    mostrarCodigoDeRecuperacao(r.codigoRecuperacao, QStringLiteral("Senha redefinida"));
    m_entrarEmail->setText(m_recEmail->text());
    irPara(PaginaEntrar);
    mostrarErro(m_entrarErro, QString());
    m_entrarErro->setText(QStringLiteral("Senha redefinida. Entre com a nova senha."));
    m_entrarErro->setVisible(true);
    ThemeManager::definirEstado(m_entrarErro, ThemeManager::Estado::Sucesso);
}

void LoginDialog::concluir(const Conta &conta)
{
    m_conta = conta;
    accept();
}

// Mostra o código de recuperação UMA vez e só deixa continuar depois que a pessoa confirma que guardou.
void LoginDialog::mostrarCodigoDeRecuperacao(const QString &codigo, const QString &titulo)
{
    QDialog dlg(this);
    dlg.setWindowTitle(titulo);
    dlg.setModal(true);
    dlg.setMinimumWidth(460);

    auto *layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(28, 24, 28, 22);
    layout->setSpacing(12);

    auto *t = new QLabel(titulo);
    t->setObjectName(QStringLiteral("tituloLogin"));
    auto *texto = new QLabel(QStringLiteral(
        "Este é o seu <b>código de recuperação</b>. Se esquecer a senha, é a única forma de voltar a entrar "
        "e ele <b>não será mostrado de novo</b>. Anote em papel ou guarde num lugar seguro."));
    texto->setWordWrap(true);
    texto->setTextFormat(Qt::RichText);

    auto *mostrador = new QLabel(codigo);
    mostrador->setObjectName(QStringLiteral("codigoRecuperacao"));
    mostrador->setAlignment(Qt::AlignCenter);
    mostrador->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *copiar = new QPushButton(QStringLiteral("Copiar código"));
    copiar->setAutoDefault(false);
    connect(copiar, &QPushButton::clicked, &dlg, [codigo, copiar] {
        QApplication::clipboard()->setText(codigo);
        copiar->setText(QStringLiteral("Copiado!"));
    });

    auto *guardei = new QCheckBox(QStringLiteral("Guardei o código em um lugar seguro"));
    auto *continuar = new QPushButton(QStringLiteral("Continuar"));
    continuar->setObjectName(QStringLiteral("primary"));
    continuar->setEnabled(false);
    continuar->setDefault(true);
    connect(guardei, &QCheckBox::toggled, continuar, &QWidget::setEnabled);
    connect(continuar, &QPushButton::clicked, &dlg, &QDialog::accept);

    layout->addWidget(t);
    layout->addWidget(texto);
    layout->addWidget(mostrador);
    layout->addWidget(copiar, 0, Qt::AlignLeft);
    layout->addSpacing(4);
    layout->addWidget(guardei);
    layout->addWidget(continuar, 0, Qt::AlignRight);

    dlg.exec();
}

// ============================================================================
// Ícones, arrasto, abertura
// ============================================================================

void LoginDialog::atualizarIcones()
{
    for (const auto &par : std::as_const(m_iconesDeCampos))
        par.first->setIcon(ThemeManager::iconeColorido(par.second, Tokens::Id::InkMuted, 18));
    // Ícones do painel de marca (cor on-primary do tema atual)
    for (QLabel *rotulo : findChildren<QLabel *>()) {
        const QVariant nome = rotulo->property("iconeDaMarca");
        if (nome.isValid())
            rotulo->setPixmap(ThemeManager::pixmap(nome.toString(), ThemeManager::cor(Tokens::Id::OnPrimary), 22));
    }
    for (QAction *olho : std::as_const(m_olhos)) {
        const auto *campo = qobject_cast<QLineEdit *>(olho->parent());
        const bool visivel = campo && campo->echoMode() == QLineEdit::Normal;
        olho->setIcon(ThemeManager::iconeColorido(visivel ? QStringLiteral("olho-fechado") : QStringLiteral("olho"),
                                                  Tokens::Id::InkMuted, 18));
    }
}

void LoginDialog::mousePressEvent(QMouseEvent *evento)
{
    // Arrasta a janela por qualquer área livre (como não há barra de título).
    if (evento->button() == Qt::LeftButton && windowHandle()) {
        windowHandle()->startSystemMove();
        evento->accept();
        return;
    }
    QDialog::mousePressEvent(evento);
}

void LoginDialog::showEvent(QShowEvent *evento)
{
    QDialog::showEvent(evento);
    IconeDaJanela::aplicar(this);
    static bool animado = false;  // só na primeira abertura (não a cada logout)
    if (!animado) {
        animado = true;
        setWindowOpacity(0.0);
        auto *animacao = new QPropertyAnimation(this, "windowOpacity", this);
        animacao->setDuration(260);
        animacao->setStartValue(0.0);
        animacao->setEndValue(1.0);
        animacao->setEasingCurve(QEasingCurve::OutCubic);
        animacao->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void LoginDialog::keyPressEvent(QKeyEvent *evento)
{
    if (evento->key() == Qt::Key_Escape) {
        reject();
        return;
    }
    QDialog::keyPressEvent(evento);
}
