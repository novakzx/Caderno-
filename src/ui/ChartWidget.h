#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <QWidget>

// Gráfico simples desenhado à mão (barras ou linha), sem depender do módulo
// Qt Charts. Serve para médias, distribuição de notas e frequência.
class ChartWidget : public QWidget {
    Q_OBJECT
public:
    enum class Tipo { Barras, Linha };

    struct Ponto {
        QString rotulo;
        double valor = 0.0;
        bool valido = true;  // false = sem dado (aparece como "—" e não entra na linha)
    };

    explicit ChartWidget(QWidget *parent = nullptr);

    // maximo: valor do topo do eixo Y. limiar >= 0 desenha uma linha tracejada
    // (ex.: nota de corte) e pinta de vermelho as barras abaixo dela.
    void setDados(const QList<Ponto> &pontos, Tipo tipo, const QString &titulo, double maximo,
                  double limiar = -1.0, const QString &sufixo = QString(), int casasDecimais = 1);

    QSize sizeHint() const override { return QSize(640, 380); }
    QSize minimumSizeHint() const override { return QSize(360, 260); }

protected:
    void paintEvent(QPaintEvent *evento) override;
    void changeEvent(QEvent *evento) override;

private:
    QString formatar(double valor) const;

    QList<Ponto> m_pontos;
    Tipo m_tipo = Tipo::Barras;
    QString m_titulo;
    double m_maximo = 10.0;
    double m_limiar = -1.0;
    QString m_sufixo;
    int m_casas = 1;
};
