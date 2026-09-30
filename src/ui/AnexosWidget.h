#pragma once

#include <QWidget>

class AnexoRepository;
class QListWidget;
class QPushButton;

// Abre um arquivo no programa padrão do sistema (PowerPoint, leitor de PDF...).
// Mostra uma mensagem se o arquivo não existe, não abre ou é um executável
// (executáveis e scripts nunca são abertos por aqui).
bool abrirAnexo(QWidget *parent, const QString &caminho);

// Lista de arquivos anexados a uma turma ou a uma aula, com adicionar, abrir e
// remover. Reutilizado na tela de Aulas (anexos da aula) e na aba "Arquivos"
// da tela de Turmas (todos os anexos da turma).
class AnexosWidget : public QWidget {
    Q_OBJECT
public:
    explicit AnexosWidget(AnexoRepository &anexos, QWidget *parent = nullptr);

    // aulaId > 0: anexos dessa aula (novos arquivos ficam ligados à aula e à turma).
    // aulaId == 0: todos os anexos da turma (novos ficam só ligados à turma).
    // turmaId == 0: sem contexto (lista vazia e botões desativados).
    void definirContexto(int turmaId, int aulaId = 0);
    void recarregar();

private:
    void adicionar();
    void abrirSelecionado();
    void removerSelecionado();
    void atualizarBotoes();
    int anexoSelecionadoId() const;

    AnexoRepository &m_anexos;
    int m_turmaId = 0;
    int m_aulaId = 0;

    QListWidget *m_lista = nullptr;
    QPushButton *m_btnAdicionar = nullptr;
    QPushButton *m_btnAbrir = nullptr;
    QPushButton *m_btnRemover = nullptr;
};
