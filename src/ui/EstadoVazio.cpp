#include "ui/EstadoVazio.h"

#include "ui/ThemeManager.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QEvent>
#include <QLabel>
#include <QVBoxLayout>

EstadoVazio::EstadoVazio(const QString &icone, const QString &titulo, const QString &dica, QWidget *parent)
    : QWidget(parent), m_icone(icone)
{
    setObjectName(QStringLiteral("estadoVazio"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(8);
    layout->addStretch(1);

    m_lblIcone = new QLabel;
    m_lblIcone->setAlignment(Qt::AlignCenter);
    m_lblIcone->setFixedHeight(44);
    layout->addWidget(m_lblIcone);

    m_lblTitulo = new QLabel;
    m_lblTitulo->setObjectName(QStringLiteral("estadoVazioTitulo"));
    m_lblTitulo->setAlignment(Qt::AlignCenter);
    m_lblTitulo->setTextFormat(Qt::PlainText);
    m_lblTitulo->setWordWrap(true);
    layout->addWidget(m_lblTitulo);

    m_lblDica = new QLabel;
    m_lblDica->setObjectName(QStringLiteral("muted"));
    m_lblDica->setAlignment(Qt::AlignCenter);
    m_lblDica->setTextFormat(Qt::PlainText);
    m_lblDica->setWordWrap(true);
    layout->addWidget(m_lblDica);
    layout->addStretch(2);

    definirTextos(titulo, dica);
    atualizarIcone();
    connect(&ThemeManager::notificador(), &ThemeNotifier::temaMudou, this, &EstadoVazio::atualizarIcone);
}

void EstadoVazio::definirTextos(const QString &titulo, const QString &dica)
{
    m_lblTitulo->setText(titulo);
    m_lblDica->setText(dica);
    m_lblDica->setVisible(!dica.isEmpty());
}

void EstadoVazio::atualizarIcone()
{
    m_lblIcone->setPixmap(ThemeManager::pixmap(m_icone, ThemeManager::cor(Tokens::Id::InkMuted), 40));
}

EstadoVazio *EstadoVazio::sobre(QAbstractScrollArea *area, const QString &icone, const QString &titulo, const QString &dica)
{
    auto *estado = new EstadoVazio(icone, titulo, dica, area->viewport());
    estado->m_area = area;
    estado->setAttribute(Qt::WA_TransparentForMouseEvents, true);  // cliques continuam chegando na lista
    area->viewport()->installEventFilter(estado);
    estado->setGeometry(area->viewport()->rect());

    if (auto *itens = qobject_cast<QAbstractItemView *>(area); itens && itens->model()) {
        const QAbstractItemModel *modelo = itens->model();
        const auto atualizar = [estado] { estado->atualizarVisibilidade(); };
        connect(modelo, &QAbstractItemModel::rowsInserted, estado, atualizar);
        connect(modelo, &QAbstractItemModel::rowsRemoved, estado, atualizar);
        connect(modelo, &QAbstractItemModel::modelReset, estado, atualizar);
        connect(modelo, &QAbstractItemModel::layoutChanged, estado, atualizar);
    }
    estado->atualizarVisibilidade();
    return estado;
}

void EstadoVazio::atualizarVisibilidade()
{
    const auto *itens = qobject_cast<QAbstractItemView *>(m_area);
    const bool vazio = itens && itens->model() && itens->model()->rowCount(itens->rootIndex()) == 0;
    setVisible(vazio);
    if (vazio)
        raise();
}

bool EstadoVazio::eventFilter(QObject *objeto, QEvent *evento)
{
    if (m_area && objeto == m_area->viewport() && evento->type() == QEvent::Resize)
        setGeometry(m_area->viewport()->rect());
    return QWidget::eventFilter(objeto, evento);
}
