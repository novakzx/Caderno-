#pragma once

#include <QWidget>

class AlunoRepository;
class AvaliacaoRepository;
class NotasTableModel;
class NotaRepository;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QTableView;
class QTimer;
class TurmaRepository;

// Tela "Notas": planilha editável (alunos x avaliações) com média ponderada
// automática, cópia/cola do Excel e importação/exportação de .xlsx.
class NotasPage : public QWidget {
    Q_OBJECT
public:
    NotasPage(TurmaRepository &turmas, AlunoRepository &alunos, AvaliacaoRepository &avaliacoes,
              NotaRepository &notas, QWidget *parent = nullptr);

protected:
    // A cada vez que a tela aparece, relê as turmas (podem ter mudado na tela Turmas).
    void showEvent(QShowEvent *evento) override;
    bool eventFilter(QObject *objeto, QEvent *evento) override;

private:
    void recarregarTurmas();
    void recarregarModelo(int avaliacaoParaSelecionar = 0);
    void ajustarColunas();
    void preencherLargura();  // a coluna do nome ocupa a largura que sobra
    void atualizarEstado();

    int turmaAtualId() const;
    int periodoAtual() const;
    int avaliacaoAtualId() const;

    void novaAvaliacao();
    void editarAvaliacao();
    void excluirAvaliacao();
    void moverAvaliacao(int delta);

    void copiarSelecao();
    void colarSelecao();
    void limparSelecao();

    void exportarExcel();
    void importarExcel();

    void mostrarMensagem(const QString &texto, bool erro);

    TurmaRepository &m_turmas;
    AlunoRepository &m_alunos;
    AvaliacaoRepository &m_avaliacoes;
    NotaRepository &m_notas;

    NotasTableModel *m_modelo = nullptr;
    QTableView *m_tabela = nullptr;
    QComboBox *m_comboTurma = nullptr;
    QComboBox *m_comboPeriodo = nullptr;
    QDoubleSpinBox *m_notaCorte = nullptr;
    QPushButton *m_btnNovaAvaliacao = nullptr;
    QPushButton *m_btnEditar = nullptr;
    QPushButton *m_btnExcluir = nullptr;
    QPushButton *m_btnEsquerda = nullptr;
    QPushButton *m_btnDireita = nullptr;
    QPushButton *m_btnImportar = nullptr;
    QPushButton *m_btnExportar = nullptr;
    QLabel *m_dica = nullptr;
    QLabel *m_resumo = nullptr;
    QLabel *m_mensagem = nullptr;
    QTimer *m_timerMensagem = nullptr;
};
