#pragma once

#include <QTextListFormat>
#include <QWidget>

class AlunoRepository;
class AnotacaoRepository;
class AulaRepository;
struct Repositorios;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTextEdit;
class QTimer;
class QToolButton;
class TurmaRepository;

// Tela "Anotações": editor de texto rico (negrito, itálico, sublinhado, marca-texto,
// listas), com tags e vínculo opcional a turma, aluno e aula. Salva sozinho
// pouco depois de parar de digitar.
class AnotacoesPage : public QWidget {
    Q_OBJECT
public:
    explicit AnotacoesPage(Repositorios &repos, QWidget *parent = nullptr);

    // Abre uma anotação (usado pela busca global e pela aba "Anotações" da turma).
    void selecionarAnotacao(int anotacaoId);

protected:
    void showEvent(QShowEvent *evento) override;
    void hideEvent(QHideEvent *evento) override;

private:
    void recarregarFiltros();
    void recarregarLista(int selecionarId = 0, bool recarregarEditor = true);
    void carregar(int anotacaoId);
    void limparEditor();
    void preencherVinculos(int turmaId, int alunoId, int aulaId);
    void atualizarCombosDaTurma();

    bool salvarAgora();
    void agendarSalvamento();
    void marcarSujo();
    void nova();
    void excluir();

    // Formatação
    void alternarNegrito(bool ligado);
    void alternarItalico(bool ligado);
    void alternarSublinhado(bool ligado);
    void alternarMarcaTexto(bool ligado);
    void alternarLista(QTextListFormat::Style estilo);
    void limparFormatacao();
    void atualizarBotoesDeFormato();
    QToolButton *criarBotao(const QString &texto, const QString &dica, bool marcavel);

    AnotacaoRepository &m_anotacoes;
    TurmaRepository &m_turmas;
    AlunoRepository &m_alunos;
    AulaRepository &m_aulas;

    int m_id = 0;              // anotação aberta (0 = nenhuma)
    bool m_sujo = false;
    bool m_carregando = false;

    // Lista e filtros
    QLineEdit *m_busca = nullptr;
    QComboBox *m_filtroTurma = nullptr;
    QComboBox *m_filtroTag = nullptr;
    QListWidget *m_lista = nullptr;
    QPushButton *m_btnNova = nullptr;

    // Editor
    QWidget *m_editor = nullptr;
    QLabel *m_vazio = nullptr;
    QLineEdit *m_titulo = nullptr;
    QComboBox *m_turma = nullptr;
    QComboBox *m_aluno = nullptr;
    QComboBox *m_aula = nullptr;
    QLineEdit *m_tags = nullptr;
    QTextEdit *m_texto = nullptr;
    QLabel *m_estado = nullptr;
    QPushButton *m_btnExcluir = nullptr;
    QToolButton *m_btnNegrito = nullptr;
    QToolButton *m_btnItalico = nullptr;
    QToolButton *m_btnSublinhado = nullptr;
    QToolButton *m_btnMarca = nullptr;
    QTimer *m_timerSalvar = nullptr;
};
