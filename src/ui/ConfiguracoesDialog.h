#pragma once

#include "core/LembreteUtil.h"

#include <QDialog>

class GerenteDeLembretes;
class IaService;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTabWidget;

// Configurações do programa, em abas: Segurança (bloqueio por inatividade), Lembretes (notificações do Windows),
// Atualizações (consulta ao GitHub) e Assistente de IA (conta gratuita do Cloudflare Workers AI).
class ConfiguracoesDialog : public QDialog {
    Q_OBJECT
public:
    enum class Aba { Seguranca = 0, Lembretes = 1, Atualizacoes = 2, Ia = 3 };

    ConfiguracoesDialog(GerenteDeLembretes &gerente, Aba inicial, QWidget *parent = nullptr);

private:
    QWidget *criarAbaSeguranca();
    QWidget *criarAbaLembretes();
    QWidget *criarAbaAtualizacoes();
    QWidget *criarAbaIa();

    void salvar();
    bool salvarIa();
    void verificarAgora();
    void testarIa();

    GerenteDeLembretes &m_gerente;
    QTabWidget *m_abas = nullptr;

    // Segurança
    QComboBox *m_bloqueio = nullptr;
    // Lembretes
    QCheckBox *m_lembretesAtivos = nullptr;
    QComboBox *m_antecedencia = nullptr;
    QCheckBox *m_prazos = nullptr;
    // Atualizações
    QCheckBox *m_verificarAtualizacoes = nullptr;
    QLabel *m_resultadoAtualizacao = nullptr;
    QPushButton *m_botaoVerificar = nullptr;
    // IA
    QLineEdit *m_iaConta = nullptr;
    QLineEdit *m_iaToken = nullptr;
    QComboBox *m_iaModelo = nullptr;
    QLabel *m_iaEstadoDoToken = nullptr;
    QLabel *m_iaResultado = nullptr;
    QPushButton *m_botaoTestarIa = nullptr;
    IaService *m_ia = nullptr;
};
