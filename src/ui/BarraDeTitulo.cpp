#include "ui/BarraDeTitulo.h"

#include "ui/BotoesAnimados.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QWindow>

BarraDeTitulo::BarraDeTitulo(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("barraTitulo"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(36);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 0, 0, 0);
    layout->setSpacing(0);

    m_texto = new QLabel;
    m_texto->setObjectName(QStringLiteral("versao"));
    m_texto->setAttribute(Qt::WA_TransparentForMouseEvents, true);  // o arrasto passa para a barra
    layout->addWidget(m_texto);
    layout->addStretch(1);

    auto *minimizar = new BotaoJanela(QStringLiteral("janela-minimizar"), false);
    m_maximizar = new BotaoJanela(QStringLiteral("janela-maximizar"), false);
    auto *fechar = new BotaoJanela(QStringLiteral("janela-fechar"), true);
    minimizar->setToolTip(QStringLiteral("Minimizar"));
    m_maximizar->setToolTip(QStringLiteral("Maximizar"));
    fechar->setToolTip(QStringLiteral("Fechar"));
    layout->addWidget(minimizar);
    layout->addWidget(m_maximizar);
    layout->addWidget(fechar);

    connect(minimizar, &QAbstractButton::clicked, this, [this] { window()->showMinimized(); });
    connect(m_maximizar, &QAbstractButton::clicked, this, &BarraDeTitulo::alternarMaximizada);
    connect(fechar, &QAbstractButton::clicked, this, [this] { window()->close(); });
}

void BarraDeTitulo::definirMaximizada(bool maximizada)
{
    m_maximizar->definirIcone(maximizada ? QStringLiteral("janela-restaurar") : QStringLiteral("janela-maximizar"));
    m_maximizar->setToolTip(maximizada ? QStringLiteral("Restaurar") : QStringLiteral("Maximizar"));
}

void BarraDeTitulo::definirTexto(const QString &texto)
{
    m_texto->setText(texto);
}

void BarraDeTitulo::alternarMaximizada()
{
    QWidget *janela = window();
    if (janela->isMaximized())
        janela->showNormal();
    else
        janela->showMaximized();
}

void BarraDeTitulo::mousePressEvent(QMouseEvent *evento)
{
    // Arrastar pelo próprio sistema: mantém o "encaixe" nas bordas da tela do Windows.
    if (evento->button() == Qt::LeftButton && window()->windowHandle()) {
        window()->windowHandle()->startSystemMove();
        evento->accept();
        return;
    }
    QWidget::mousePressEvent(evento);
}

void BarraDeTitulo::mouseDoubleClickEvent(QMouseEvent *evento)
{
    if (evento->button() == Qt::LeftButton) {
        alternarMaximizada();
        evento->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(evento);
}
