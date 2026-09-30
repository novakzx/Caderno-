#include "ui/ChartWidget.h"

#include "ui/ThemeManager.h"

#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

namespace {

struct Cores {
    QColor fundo, grade, texto, textoSuave, destaque, alerta;
};

// Cores dos tokens do tema atual (o gráfico pinta tudo sozinho e repinta quando o tema muda).
Cores coresDoTema()
{
    using T = Tokens::Id;
    return {ThemeManager::cor(T::Surface200), ThemeManager::cor(T::Line),    ThemeManager::cor(T::Ink),
            ThemeManager::cor(T::InkMuted),   ThemeManager::cor(T::Primary), ThemeManager::cor(T::Danger)};
}

}  // namespace

ChartWidget::ChartWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void ChartWidget::setDados(const QList<Ponto> &pontos, Tipo tipo, const QString &titulo, double maximo,
                           double limiar, const QString &sufixo, int casasDecimais)
{
    m_pontos = pontos;
    m_tipo = tipo;
    m_titulo = titulo;
    m_maximo = maximo > 0.0 ? maximo : 1.0;
    m_limiar = limiar;
    m_sufixo = sufixo;
    m_casas = casasDecimais;
    update();
}

QString ChartWidget::formatar(double valor) const
{
    return QString::number(valor, 'f', m_casas).replace(QLatin1Char('.'), QLatin1Char(',')) + m_sufixo;
}

void ChartWidget::changeEvent(QEvent *evento)
{
    if (evento->type() == QEvent::StyleChange || evento->type() == QEvent::PaletteChange)
        update();  // troca de tema
    QWidget::changeEvent(evento);
}

void ChartWidget::paintEvent(QPaintEvent *)
{
    const Cores c = coresDoTema();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), c.fundo);

    // Título
    QFont negrito = font();
    negrito.setBold(true);
    negrito.setPixelSize(16);  // título do gráfico (a fonte do app é em pixels)
    p.setFont(negrito);
    p.setPen(c.texto);
    p.drawText(QRect(16, 8, width() - 32, 26), Qt::AlignLeft | Qt::AlignVCenter, m_titulo);
    p.setFont(font());

    const int n = m_pontos.size();
    if (n == 0) {
        p.setPen(c.textoSuave);
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("Sem dados para mostrar."));
        return;
    }

    // Área de desenho (deixa espaço para o eixo Y e para os rótulos inclinados do eixo X)
    const QFontMetrics fm(font());
    int maiorRotulo = 0;
    for (const Ponto &pt : m_pontos)
        maiorRotulo = std::max(maiorRotulo, fm.horizontalAdvance(pt.rotulo.left(16)));
    const bool inclinar = n > 6 || maiorRotulo > (width() - 70) / std::max(1, n) - 4;
    const int margemInferior = inclinar ? std::min(110, 24 + int(maiorRotulo * 0.7)) : 30;

    const QRect area(56, 44, width() - 56 - 18, height() - 44 - margemInferior);
    if (area.width() < 40 || area.height() < 40)
        return;

    // Grade horizontal e eixo Y
    const int passos = 5;
    p.setFont(font());
    for (int i = 0; i <= passos; ++i) {
        const double valor = m_maximo * i / passos;
        const int y = area.bottom() - int(area.height() * i / double(passos));
        p.setPen(QPen(c.grade, 1));
        p.drawLine(area.left(), y, area.right(), y);
        p.setPen(c.textoSuave);
        // Sem casas decimais quando os degraus do eixo são números inteiros.
        const bool degrauInteiro = std::fmod(m_maximo, double(passos)) == 0.0;
        p.drawText(QRect(0, y - 9, area.left() - 6, 18), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(valor, 'f', degrauInteiro ? 0 : 1).replace(QLatin1Char('.'), QLatin1Char(',')));
    }

    auto yDoValor = [&](double v) {
        return area.bottom() - int(area.height() * std::clamp(v / m_maximo, 0.0, 1.0));
    };

    const double larguraSlot = area.width() / double(n);

    // Rótulos do eixo X
    p.setPen(c.textoSuave);
    for (int i = 0; i < n; ++i) {
        const QString rotulo = fm.elidedText(m_pontos.at(i).rotulo, Qt::ElideRight, inclinar ? 140 : int(larguraSlot) - 4);
        const double cx = area.left() + larguraSlot * (i + 0.5);
        if (inclinar) {
            p.save();
            p.translate(cx - 4, area.bottom() + 10);
            p.rotate(40);
            p.drawText(0, 0, rotulo);
            p.restore();
        } else {
            p.drawText(QRectF(cx - larguraSlot / 2, area.bottom() + 6, larguraSlot, 20), Qt::AlignCenter, rotulo);
        }
    }

    if (m_tipo == Tipo::Barras) {
        const double larguraBarra = std::min(64.0, larguraSlot * 0.68);
        for (int i = 0; i < n; ++i) {
            const Ponto &pt = m_pontos.at(i);
            const double cx = area.left() + larguraSlot * (i + 0.5);
            if (!pt.valido) {
                p.setPen(c.textoSuave);
                p.drawText(QRectF(cx - larguraSlot / 2, area.bottom() - 20, larguraSlot, 18), Qt::AlignCenter, QStringLiteral("—"));
                continue;
            }
            const int topo = yDoValor(pt.valor);
            const QRectF barra(cx - larguraBarra / 2, topo, larguraBarra, std::max(1, area.bottom() - topo));
            const bool abaixo = m_limiar >= 0.0 && pt.valor < m_limiar;
            p.setPen(Qt::NoPen);
            p.setBrush(abaixo ? c.alerta : c.destaque);
            QPainterPath caminho;
            caminho.addRoundedRect(barra, 4, 4);
            p.drawPath(caminho);

            p.setPen(c.texto);
            p.drawText(QRectF(cx - larguraSlot / 2, topo - 20, larguraSlot, 18), Qt::AlignCenter, formatar(pt.valor));
        }
    } else {
        // Linha: liga pontos válidos consecutivos
        QPointF anterior;
        bool temAnterior = false;
        p.setBrush(c.destaque);
        for (int i = 0; i < n; ++i) {
            const Ponto &pt = m_pontos.at(i);
            if (!pt.valido) {
                temAnterior = false;
                continue;
            }
            const QPointF atual(area.left() + larguraSlot * (i + 0.5), yDoValor(pt.valor));
            if (temAnterior) {
                p.setPen(QPen(c.destaque, 2.5));
                p.drawLine(anterior, atual);
            }
            anterior = atual;
            temAnterior = true;
        }
        for (int i = 0; i < n; ++i) {
            const Ponto &pt = m_pontos.at(i);
            if (!pt.valido)
                continue;
            const QPointF centro(area.left() + larguraSlot * (i + 0.5), yDoValor(pt.valor));
            const bool abaixo = m_limiar >= 0.0 && pt.valor < m_limiar;
            p.setPen(QPen(c.fundo, 2));
            p.setBrush(abaixo ? c.alerta : c.destaque);
            p.drawEllipse(centro, 5, 5);
            p.setPen(c.texto);
            p.drawText(QRectF(centro.x() - 30, centro.y() - 24, 60, 18), Qt::AlignCenter, formatar(pt.valor));
        }
    }

    // Linha de corte (tracejada)
    if (m_limiar >= 0.0 && m_limiar <= m_maximo) {
        const int y = yDoValor(m_limiar);
        p.setPen(QPen(c.alerta, 1.5, Qt::DashLine));
        p.drawLine(area.left(), y, area.right(), y);
        p.setPen(c.alerta);
        p.drawText(QRect(area.right() - 120, y - 18, 118, 16), Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("mínimo %1").arg(formatar(m_limiar)));
    }
}
