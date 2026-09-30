#pragma once

#include <QWidget>

class BotaoJanela;

// Barra superior da janela sem moldura do sistema: área para arrastar (duplo clique
// maximiza) e os botões de minimizar, maximizar/restaurar e fechar, no canto direito.
// Tem a mesma cor do fundo da página, então parece que só os botões flutuam.
class BarraDeTitulo : public QWidget {
    Q_OBJECT
public:
    explicit BarraDeTitulo(QWidget *parent = nullptr);

    // Atualiza o ícone de maximizar/restaurar conforme o estado da janela.
    void definirMaximizada(bool maximizada);
    void definirTexto(const QString &texto);

protected:
    void mousePressEvent(QMouseEvent *evento) override;
    void mouseDoubleClickEvent(QMouseEvent *evento) override;

private:
    void alternarMaximizada();

    BotaoJanela *m_maximizar = nullptr;
    class QLabel *m_texto = nullptr;
};
