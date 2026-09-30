#pragma once

#include <QStackedWidget>

// QStackedWidget que faz a nova página "aparecer" (fade-in curto) ao trocar de seção.
// A primeira exibição e trocas com a janela oculta não animam.
class PilhaAnimada : public QStackedWidget {
    Q_OBJECT
public:
    using QStackedWidget::QStackedWidget;

    void irPara(int indice);
};
