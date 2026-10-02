#pragma once

#include "services/IaPrompts.h"

#include <QWidget>

class AnotacaoRepository;
class AulaRepository;
class IaService;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class TurmaRepository;
struct Repositorios;
struct ResultadoIa;

// Tela "Assistente": gera textos para a aula com IA (plano de aula, questões, atividades, comunicados...).
// Usa a conta gratuita do Cloudflare Workers AI do próprio professor (Configurações > Assistente de IA).
// A resposta chega aos poucos; pode ser editada, copiada, salva como anotação ou virar um plano de aula.
//
// Privacidade: só vai para o serviço o que o professor digita nos campos. Nada do banco é anexado.
class AssistentePage : public QWidget {
    Q_OBJECT
public:
    explicit AssistentePage(Repositorios &repos, QWidget *parent = nullptr);

signals:
    // Botão "Configurar": a janela principal abre as Configurações na aba da IA.
    void configurarSolicitado();

protected:
    void showEvent(QShowEvent *evento) override;
    void hideEvent(QHideEvent *evento) override;

private:
    void atualizarCampos();
    void atualizarAviso();
    void turmaMudou();
    void gerar();
    void parar();
    void aoConcluir(const ResultadoIa &resultado);
    bool confirmarEnvio();
    void definirOcupado(bool ocupado);
    IaPrompts::Pedido pedidoAtual() const;
    int turmaEscolhida() const;
    QString tituloSugerido() const;

    void copiar();
    void salvarComoAnotacao();
    void criarPlanoDeAula();

    TurmaRepository &m_turmas;
    AnotacaoRepository &m_anotacoes;
    AulaRepository &m_aulas;
    IaService *m_ia = nullptr;

    QFrame *m_aviso = nullptr;
    QLabel *m_textoAviso = nullptr;
    QComboBox *m_tarefa = nullptr;
    QComboBox *m_turma = nullptr;
    QLineEdit *m_disciplina = nullptr;
    QLineEdit *m_serie = nullptr;
    QLineEdit *m_tema = nullptr;
    QComboBox *m_duracao = nullptr;
    QSpinBox *m_quantidade = nullptr;
    QPlainTextEdit *m_detalhes = nullptr;
    QPushButton *m_botaoGerar = nullptr;

    QLabel *m_status = nullptr;
    QProgressBar *m_progresso = nullptr;
    QPlainTextEdit *m_resultado = nullptr;
    QPushButton *m_botaoCopiar = nullptr;
    QPushButton *m_botaoAnotacao = nullptr;
    QPushButton *m_botaoAula = nullptr;
    QPushButton *m_botaoLimpar = nullptr;

    // Rótulos dos campos (para esconder os que a tarefa não usa).
    QList<QWidget *> m_camposDisciplina, m_camposTema, m_camposDuracao, m_camposQuantidade;
    QLabel *m_rotuloDetalhes = nullptr;
    bool m_ocupado = false;
};
