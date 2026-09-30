#pragma once

#include "database/BuscaRepository.h"

#include <QDialog>
#include <QList>
#include <optional>

class QLabel;
class QLineEdit;
class QListWidget;

// Busca global (Ctrl+K): uma caixa de texto e uma lista de resultados de todo o
// programa (turmas, alunos, anotações, aulas, tarefas, eventos e arquivos).
// Ignora acentos e maiúsculas; todas as palavras digitadas precisam aparecer.
class BuscaDialog : public QDialog {
    Q_OBJECT
public:
    explicit BuscaDialog(BuscaRepository &busca, QWidget *parent = nullptr);

    // Resultado escolhido pelo usuário (vazio se cancelou).
    std::optional<ItemBusca> escolhido() const { return m_escolhido; }

protected:
    bool eventFilter(QObject *alvo, QEvent *evento) override;

private:
    void filtrar();
    void confirmar();

    QList<ItemBusca> m_indice;
    QList<ItemBusca> m_resultados;  // mesma ordem da lista mostrada
    std::optional<ItemBusca> m_escolhido;

    QLineEdit *m_campo = nullptr;
    QListWidget *m_lista = nullptr;
    QLabel *m_dica = nullptr;
};
