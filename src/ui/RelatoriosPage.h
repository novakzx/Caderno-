#pragma once

#include <QWidget>

class ChartWidget;
class DesempenhoService;
struct Repositorios;
class QComboBox;
class QLabel;
class TurmaRepository;
class AlunoRepository;

// Tela "Relatórios": gráficos de desempenho da turma (médias, distribuição,
// frequência, evolução de um aluno) e geração de relatórios em PDF.
class RelatoriosPage : public QWidget {
    Q_OBJECT
public:
    explicit RelatoriosPage(Repositorios &repos, QWidget *parent = nullptr);
    ~RelatoriosPage() override;

protected:
    void showEvent(QShowEvent *evento) override;

private:
    enum Grafico { MediaPorAluno, MediaPorAvaliacao, DistribuicaoDeMedias, FrequenciaPorAluno, EvolucaoDoAluno };

    void recarregarTurmas();
    void recarregarAlunos();
    void atualizarGrafico();
    double notaDeCorte() const;
    int turmaAtualId() const;
    int periodoAtual() const;
    int alunoAtualId() const;

    void salvarGraficoPng();
    void gerarBoletim();
    void gerarFrequencia();
    void gerarFicha();
    // Pergunta onde salvar, grava o PDF e oferece abri-lo.
    void salvarEAbrirPdf(const QString &html, const QString &nomeSugerido, const QString &titulo, bool paisagem);

    TurmaRepository &m_turmas;
    AlunoRepository &m_alunos;
    DesempenhoService *m_servico = nullptr;

    QComboBox *m_comboTurma = nullptr;
    QComboBox *m_comboPeriodo = nullptr;
    QComboBox *m_comboGrafico = nullptr;
    QComboBox *m_comboAluno = nullptr;
    ChartWidget *m_grafico = nullptr;
    QLabel *m_resumo = nullptr;
    bool m_carregando = false;
};
