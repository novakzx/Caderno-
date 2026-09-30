#pragma once

#include "services/ContaService.h"

#include <QDialog>
#include <QList>
#include <QPair>

class BarraDeForca;
class BotaoJanela;
class PilhaAnimada;
class QLabel;
class QLineEdit;
class QPushButton;

// Janela de entrada do programa: Entrar, Criar conta e Recuperar senha (tudo local, sem internet).
// Sem a moldura do sistema; painel de marca à esquerda, formulários à direita.
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    // `haDadosAntigos` = já existe um banco de dados de antes das contas: a primeira conta criada fica com ele.
    LoginDialog(ContaService &contas, bool haDadosAntigos, QWidget *parent = nullptr);

    Conta conta() const { return m_conta; }

protected:
    void mousePressEvent(QMouseEvent *evento) override;
    void showEvent(QShowEvent *evento) override;
    void keyPressEvent(QKeyEvent *evento) override;

private:
    enum Pagina { PaginaEntrar = 0, PaginaCriar = 1, PaginaRecuperar = 2 };

    QWidget *criarPainelMarca();
    QWidget *criarFormEntrar();
    QWidget *criarFormCriar();
    QWidget *criarFormRecuperar();
    QLineEdit *novoCampo(const QString &dica, const QString &icone, bool senha, QWidget *pai);
    void irPara(Pagina pagina);

    void acaoEntrar();
    void acaoCriar();
    void acaoRedefinir();
    void concluir(const Conta &conta);

    void mostrarErro(QLabel *rotulo, const QString &texto);
    void tremer();
    void mostrarCodigoDeRecuperacao(const QString &codigo, const QString &titulo);
    void atualizarIcones();

    ContaService &m_contas;
    bool m_haDadosAntigos = false;
    Conta m_conta;

    PilhaAnimada *m_paginas = nullptr;

    // Entrar
    QLineEdit *m_entrarEmail = nullptr;
    QLineEdit *m_entrarSenha = nullptr;
    QLabel *m_entrarErro = nullptr;
    QPushButton *m_botaoEntrar = nullptr;

    // Criar conta
    QLineEdit *m_criarNome = nullptr;
    QLineEdit *m_criarEmail = nullptr;
    QLineEdit *m_criarSenha = nullptr;
    QLineEdit *m_criarConfirma = nullptr;
    BarraDeForca *m_forca = nullptr;
    QLabel *m_textoForca = nullptr;
    QLabel *m_criarErro = nullptr;
    QPushButton *m_botaoCriar = nullptr;

    // Recuperar senha
    QLineEdit *m_recEmail = nullptr;
    QLineEdit *m_recCodigo = nullptr;
    QLineEdit *m_recSenha = nullptr;
    QLineEdit *m_recConfirma = nullptr;
    QLabel *m_recErro = nullptr;
    QPushButton *m_botaoRecuperar = nullptr;

    QList<QPair<QAction *, QString>> m_iconesDeCampos;  // ação -> ícone, para refazer ao trocar o tema
    QList<QAction *> m_olhos;
};
