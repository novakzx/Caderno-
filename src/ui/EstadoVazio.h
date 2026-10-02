#pragma once

#include <QWidget>

class QAbstractScrollArea;
class QLabel;

// "Estado vazio": ícone, título e uma dica de o que fazer, no lugar de uma lista em branco.
//
//   EstadoVazio::sobre(tabela, "tarefas", "Nenhuma tarefa", "Use + Nova tarefa para começar.");
//
// cobre a área da lista/tabela e some sozinho quando entra a primeira linha (acompanha o modelo da lista).
// Também pode ser usado solto, como o painel "nada selecionado" dos editores.
// Os textos são fixos do programa e sempre mostrados como texto simples.
class EstadoVazio : public QWidget {
    Q_OBJECT
public:
    EstadoVazio(const QString &icone, const QString &titulo, const QString &dica, QWidget *parent = nullptr);

    // Põe o estado vazio sobre a área de rolagem (lista, tabela) e o liga ao modelo dela.
    static EstadoVazio *sobre(QAbstractScrollArea *area, const QString &icone, const QString &titulo, const QString &dica);

    void definirTextos(const QString &titulo, const QString &dica);

protected:
    bool eventFilter(QObject *objeto, QEvent *evento) override;

private:
    void atualizarIcone();
    void atualizarVisibilidade();

    QString m_icone;
    QLabel *m_lblIcone = nullptr;
    QLabel *m_lblTitulo = nullptr;
    QLabel *m_lblDica = nullptr;
    QAbstractScrollArea *m_area = nullptr;
};
