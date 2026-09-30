#pragma once

#include <QAbstractButton>
#include <QPixmap>
#include <QPointer>

class QPropertyAnimation;

// Botão que pinta a si mesmo e anima a passagem do mouse e a seleção
// (transição suave de cor, em vez de trocar de uma vez como o QSS faz).
// As subclasses só desenham; as cores vêm do ThemeManager.
class BotaoAnimado : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(qreal hover READ hover WRITE setHover)
    Q_PROPERTY(qreal marcado READ marcado WRITE setMarcado)
public:
    explicit BotaoAnimado(QWidget *parent = nullptr);

    qreal hover() const { return m_hover; }
    qreal marcado() const { return m_marcado; }
    void setHover(qreal valor);
    void setMarcado(qreal valor);

protected:
    void enterEvent(QEnterEvent *evento) override;
    void leaveEvent(QEvent *evento) override;
    void focusInEvent(QFocusEvent *evento) override;
    void focusOutEvent(QFocusEvent *evento) override;
    void changeEvent(QEvent *evento) override;

    // Só mostra o anel de foco quando o foco veio do teclado (Tab), não do clique.
    bool focoPorTeclado() const { return hasFocus() && m_foco_teclado; }
    static QColor misturar(const QColor &a, const QColor &b, qreal t);

private:
    void animar(QPropertyAnimation *&animacao, const char *propriedade, qreal destino);

    qreal m_hover = 0.0;
    qreal m_marcado = 0.0;
    bool m_foco_teclado = false;
    QPointer<QPropertyAnimation> m_animHover;
    QPointer<QPropertyAnimation> m_animMarcado;
};

// Item da barra lateral: ícone + texto, fundo em primary-soft quando ativo.
class BotaoNav : public BotaoAnimado {
    Q_OBJECT
public:
    BotaoNav(const QString &texto, const QString &icone, QWidget *parent = nullptr);

    QSize sizeHint() const override { return QSize(220, 38); }
    QSize minimumSizeHint() const override { return QSize(120, 38); }
    void definirIcone(const QString &icone);
    void recarregarIcones();  // depois de trocar o tema

protected:
    void paintEvent(QPaintEvent *evento) override;

private:
    QString m_icone;
    QPixmap m_pixmapNormal;
    QPixmap m_pixmapMarcado;
};

// Botão da barra de título (minimizar, maximizar, fechar).
class BotaoJanela : public BotaoAnimado {
    Q_OBJECT
public:
    BotaoJanela(const QString &icone, bool destrutivo, QWidget *parent = nullptr);

    QSize sizeHint() const override { return QSize(46, 36); }
    void definirIcone(const QString &icone);

protected:
    void paintEvent(QPaintEvent *evento) override;

private:
    QString m_icone;
    bool m_destrutivo = false;
};
