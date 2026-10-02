#pragma once

#include <QWidget>

class AnexosWidget;
class AulaRepository;
struct Repositorios;
class EstadoVazio;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class TurmaRepository;

// Tela "Aulas": planos de aula (tema, objetivos, materiais, observações) com
// apresentações e arquivos anexados. A lista fica à esquerda; o editor, à direita.
class AulasPage : public QWidget {
    Q_OBJECT
public:
    explicit AulasPage(Repositorios &repos, QWidget *parent = nullptr);

    // Seleciona (e mostra) um plano de aula; usado pela busca global.
    void selecionarAula(int aulaId);

protected:
    void showEvent(QShowEvent *evento) override;

private:
    void recarregarTurmas();
    void recarregarLista(int selecionarId = 0);
    void carregarNoEditor(int aulaId);
    void limparEditor();
    bool salvar();
    bool confirmarDescarteSeNecessario();
    void novoPlano();
    void excluir();
    void marcarSujo(bool sujo);
    int aulaAtualId() const { return m_aulaId; }

    AulaRepository &m_aulas;
    TurmaRepository &m_turmas;

    int m_aulaId = 0;          // aula aberta no editor (0 = nenhuma)
    bool m_sujo = false;       // há alterações não salvas?
    bool m_carregando = false; // evita marcar "sujo" enquanto preenche o editor

    QComboBox *m_filtroTurma = nullptr;
    QListWidget *m_lista = nullptr;
    QPushButton *m_btnNovo = nullptr;

    QWidget *m_editor = nullptr;
    QComboBox *m_turma = nullptr;
    QDateEdit *m_data = nullptr;
    QLineEdit *m_tema = nullptr;
    QPlainTextEdit *m_objetivos = nullptr;
    QPlainTextEdit *m_materiais = nullptr;
    QPlainTextEdit *m_observacoes = nullptr;
    AnexosWidget *m_anexos = nullptr;
    QPushButton *m_btnSalvar = nullptr;
    QPushButton *m_btnExcluir = nullptr;
    QLabel *m_estado = nullptr;
    EstadoVazio *m_vazio = nullptr;
};
