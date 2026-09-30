#include "ui/PilhaAnimada.h"

#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

void PilhaAnimada::irPara(int indice)
{
    if (indice == currentIndex())
        return;
    setCurrentIndex(indice);

    QWidget *pagina = currentWidget();
    if (!pagina || !isVisible())
        return;

    // Efeito de opacidade só durante a animação (depois é removido, para não deixar
    // a página renderizando "em cache" e mais lenta).
    auto *efeito = new QGraphicsOpacityEffect(pagina);
    efeito->setOpacity(0.0);
    pagina->setGraphicsEffect(efeito);

    auto *animacao = new QPropertyAnimation(efeito, "opacity", efeito);
    animacao->setDuration(200);
    animacao->setStartValue(0.0);
    animacao->setEndValue(1.0);
    animacao->setEasingCurve(QEasingCurve::OutCubic);
    connect(animacao, &QPropertyAnimation::finished, pagina, [pagina, efeito] {
        if (pagina->graphicsEffect() == efeito)
            pagina->setGraphicsEffect(nullptr);  // apaga o efeito
    });
    animacao->start();
}
