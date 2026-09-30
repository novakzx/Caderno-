#include "ui/BotoesAnimados.h"

#include "ui/ThemeManager.h"

#include <QEnterEvent>
#include <QFocusEvent>
#include <QPainter>
#include <QPropertyAnimation>

// ============================================================================
// BotaoAnimado
// ============================================================================

BotaoAnimado::BotaoAnimado(QWidget *parent) : QAbstractButton(parent)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);  // clique não "pega" o foco: só o Tab mostra o anel
    setAttribute(Qt::WA_Hover, true);
    connect(this, &QAbstractButton::toggled, this, [this](bool ligado) {
        QPropertyAnimation *a = m_animMarcado;
        animar(a, "marcado", ligado ? 1.0 : 0.0);
        m_animMarcado = a;
    });
}

void BotaoAnimado::setHover(qreal valor)
{
    m_hover = valor;
    update();
}

void BotaoAnimado::setMarcado(qreal valor)
{
    m_marcado = valor;
    update();
}

// Anima uma propriedade (0..1) até `destino`, parando a animação anterior dela.
void BotaoAnimado::animar(QPropertyAnimation *&animacao, const char *propriedade, qreal destino)
{
    if (animacao)
        animacao->stop();
    auto *nova = new QPropertyAnimation(this, propriedade, this);
    nova->setDuration(160);
    nova->setEasingCurve(QEasingCurve::OutCubic);
    nova->setStartValue(property(propriedade).toReal());
    nova->setEndValue(destino);
    nova->start(QAbstractAnimation::DeleteWhenStopped);
    animacao = nova;
}

void BotaoAnimado::enterEvent(QEnterEvent *evento)
{
    QAbstractButton::enterEvent(evento);
    QPropertyAnimation *a = m_animHover;
    animar(a, "hover", 1.0);
    m_animHover = a;
}

void BotaoAnimado::leaveEvent(QEvent *evento)
{
    QAbstractButton::leaveEvent(evento);
    QPropertyAnimation *a = m_animHover;
    animar(a, "hover", 0.0);
    m_animHover = a;
}

void BotaoAnimado::focusInEvent(QFocusEvent *evento)
{
    m_foco_teclado = evento->reason() == Qt::TabFocusReason || evento->reason() == Qt::BacktabFocusReason;
    QAbstractButton::focusInEvent(evento);
    update();
}

void BotaoAnimado::focusOutEvent(QFocusEvent *evento)
{
    QAbstractButton::focusOutEvent(evento);
    update();
}

void BotaoAnimado::changeEvent(QEvent *evento)
{
    if (evento->type() == QEvent::EnabledChange)
        update();
    QAbstractButton::changeEvent(evento);
}

QColor BotaoAnimado::misturar(const QColor &a, const QColor &b, qreal t)
{
    t = qBound<qreal>(0.0, t, 1.0);
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

// ============================================================================
// BotaoNav (barra lateral)
// ============================================================================

BotaoNav::BotaoNav(const QString &texto, const QString &icone, QWidget *parent) : BotaoAnimado(parent)
{
    setText(texto);
    setCheckable(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    definirIcone(icone);
}

void BotaoNav::definirIcone(const QString &icone)
{
    m_icone = icone;
    recarregarIcones();
}

void BotaoNav::recarregarIcones()
{
    m_pixmapNormal = ThemeManager::pixmap(m_icone, ThemeManager::cor(Tokens::Id::InkMuted), 20);
    m_pixmapMarcado = ThemeManager::pixmap(m_icone, ThemeManager::cor(Tokens::Id::Primary), 20);
    update();
}

void BotaoNav::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRectF area = QRectF(rect()).adjusted(10, 1, -10, -1);
    const qreal ativo = marcado();

    // Fundo: surface-300 ao passar o mouse; primary-soft quando é a seção atual.
    QColor passando = ThemeManager::cor(Tokens::Id::Surface300);
    passando.setAlphaF(hover() * (1.0 - ativo));
    QColor selecionado = ThemeManager::cor(Tokens::Id::PrimarySoft);
    selecionado.setAlphaF(ativo);
    p.setPen(Qt::NoPen);
    p.setBrush(passando);
    p.drawRoundedRect(area, 10, 10);
    p.setBrush(selecionado);
    p.drawRoundedRect(area, 10, 10);

    // Ícone: cinza quando normal, verde quando ativo (as duas versões se cruzam).
    const qreal tamanho = 20;
    const QPointF canto(area.left() + 14, area.center().y() - tamanho / 2.0);
    p.setOpacity(1.0 - ativo);
    p.drawPixmap(canto, m_pixmapNormal);
    p.setOpacity(ativo);
    p.drawPixmap(canto, m_pixmapMarcado);
    p.setOpacity(1.0);

    // Texto
    QFont fonte = font();
    fonte.setWeight(ativo > 0.5 ? QFont::DemiBold : QFont::Normal);
    p.setFont(fonte);
    QColor cor = misturar(ThemeManager::cor(Tokens::Id::InkMuted), ThemeManager::cor(Tokens::Id::Ink), hover());
    cor = misturar(cor, ThemeManager::cor(Tokens::Id::Primary), ativo);
    p.setPen(cor);
    const QRectF texto(area.left() + 14 + tamanho + 10, area.top(), area.width() - 14 - tamanho - 18, area.height());
    p.drawText(texto, Qt::AlignVCenter | Qt::AlignLeft, fontMetrics().elidedText(text(), Qt::ElideRight, int(texto.width())));

    if (focoPorTeclado()) {
        p.setPen(QPen(ThemeManager::cor(Tokens::Id::Focus), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(area.adjusted(1, 1, -1, -1), 10, 10);
    }
}

// ============================================================================
// BotaoJanela (minimizar / maximizar / fechar)
// ============================================================================

BotaoJanela::BotaoJanela(const QString &icone, bool destrutivo, QWidget *parent)
    : BotaoAnimado(parent), m_icone(icone), m_destrutivo(destrutivo)
{
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);
}

void BotaoJanela::definirIcone(const QString &icone)
{
    m_icone = icone;
    update();
}

void BotaoJanela::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Fundo: surface-300 (ou danger, no botão de fechar) ao passar o mouse.
    QColor fundo = ThemeManager::cor(m_destrutivo ? Tokens::Id::Danger : Tokens::Id::Surface300);
    fundo.setAlphaF(hover());
    p.fillRect(rect(), fundo);

    // Ícone: cinza → escuro; sobre o vermelho do "fechar", a cor de texto do botão principal.
    QColor cor = misturar(ThemeManager::cor(Tokens::Id::InkMuted), ThemeManager::cor(Tokens::Id::Ink), hover());
    if (m_destrutivo)
        cor = misturar(cor, ThemeManager::cor(Tokens::Id::OnPrimary), hover());
    const QPixmap pixmap = ThemeManager::pixmap(m_icone, cor, 18);
    p.drawPixmap(QPointF((width() - 18) / 2.0, (height() - 18) / 2.0), pixmap);
}
