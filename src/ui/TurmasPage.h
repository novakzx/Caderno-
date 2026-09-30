#pragma once

#include <QWidget>

class AlunoRepository;
class AnexosWidget;
class AnotacaoRepository;
struct Repositorios;
class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTabWidget;
class QTableWidget;
class TurmaRepository;

// Tela "Turmas": lista de turmas à esquerda e, à direita, abas da turma
// selecionada: Alunos (CRUD completo), Arquivos (apresentações e documentos
// anexados) e Anotações (as anotações ligadas à turma).
class TurmasPage : public QWidget {
    Q_OBJECT
public:
    explicit TurmasPage(Repositorios &repos, QWidget *parent = nullptr);

    // Seleciona uma turma (e, se informado, um aluno dela); usado pela busca global.
    void selecionarTurma(int turmaId, int alunoId = 0);

signals:
    // Duplo clique numa anotação da aba "Anotações": a janela principal abre a tela de anotações.
    void abrirAnotacaoSolicitada(int anotacaoId);

private:
    void atualizarAbasDaTurma();

    // --- Turmas ---
    void recarregarTurmas(int selecionarId = 0);
    int turmaSelecionadaId() const;
    void novaTurma();
    void editarTurma();
    void excluirTurma();

    // --- Alunos ---
    void recarregarAlunos(int selecionarId = 0);
    int alunoSelecionadoId() const;
    void novoAluno();
    void editarAluno();
    void excluirAluno();

    void atualizarEstadoBotoes();
    void mostrarErro(const QString &titulo, const QString &detalhe);

    TurmaRepository &m_turmas;
    AlunoRepository &m_alunos;
    AnotacaoRepository &m_anotacoes;

    QTabWidget *m_abas = nullptr;
    AnexosWidget *m_anexos = nullptr;
    QListWidget *m_listaNotas = nullptr;

    QTableWidget *m_tabelaTurmas = nullptr;
    QCheckBox *m_mostrarArquivadas = nullptr;
    QPushButton *m_btnNovaTurma = nullptr;
    QPushButton *m_btnEditarTurma = nullptr;
    QPushButton *m_btnExcluirTurma = nullptr;

    QLabel *m_tituloAlunos = nullptr;
    QLineEdit *m_busca = nullptr;
    QTableWidget *m_tabelaAlunos = nullptr;
    QPushButton *m_btnNovoAluno = nullptr;
    QPushButton *m_btnEditarAluno = nullptr;
    QPushButton *m_btnExcluirAluno = nullptr;
};
