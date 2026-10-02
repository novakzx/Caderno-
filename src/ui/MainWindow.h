#pragma once

#include "database/BuscaRepository.h"

#include <QElapsedTimer>
#include <QList>
#include <QMainWindow>
#include <QString>

class AnotacoesPage;
class AssistentePage;
class AtualizacaoService;
class AulasPage;
class BarraDeTitulo;
class BotaoNav;
class CalendarioPage;
class GerenteDeLembretes;
class PilhaAnimada;
class QButtonGroup;
class QLabel;
class QPushButton;
class QTimer;
class QVBoxLayout;
class TarefasPage;
class TurmasPage;
struct Repositorios;

// Janela principal, sem a moldura do sistema: barra lateral à esquerda (navegação, busca,
// backup, tema e conta) e área de conteúdo à direita (uma página por seção, com fade ao
// trocar), com a barra de título própria no topo.
//
// Para adicionar uma nova seção, basta chamar adicionarSecao() no construtor
// com o nome do ícone (resources/icons/<nome>.svg), o título do botão e a página.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(Repositorios &repos, const QString &nomeUsuario, const QString &emailUsuario,
               QWidget *parent = nullptr);
    ~MainWindow() override;

    // A janela foi fechada pelo bloqueio por inatividade (o login mostra um aviso).
    bool bloqueadaPorInatividade() const { return m_bloqueadaPorInatividade; }

signals:
    // O usuário pediu "Sair": a janela fecha e o programa volta para a tela de login.
    void trocarContaSolicitado();

protected:
    void closeEvent(QCloseEvent *evento) override;
    void changeEvent(QEvent *evento) override;
    void showEvent(QShowEvent *evento) override;
    bool eventFilter(QObject *objeto, QEvent *evento) override;

private:
    void construirBarraLateral(QWidget *barra, const QString &nomeUsuario, const QString &emailUsuario);
    BotaoNav *novoBotaoDoRodape(const QString &icone, const QString &texto);
    void adicionarSecao(const QString &icone, const QString &titulo, QWidget *pagina);
    void atualizarAparencia();
    void irParaSecao(int indice);
    void irParaPagina(QWidget *pagina);

    void abrirBusca();
    void navegarPara(const ItemBusca &item);
    void abrirBackup();
    void abrirSobre();
    void abrirConfiguracoes(int aba);  // aba = ConfiguracoesDialog::Aba
    void verificarInatividade();
    void verificarAtualizacao();
    void trazerParaFrente();
    void verificarBackupAutomatico(int intervaloHoras);

    // Redimensionar pelas bordas (a janela não tem moldura): detecta a borda sob o mouse.
    Qt::Edges bordaEm(const QPoint &posicaoGlobal) const;
    void atualizarCursorDaBorda(Qt::Edges borda);

    Repositorios &m_repos;

    BarraDeTitulo *m_barraTitulo = nullptr;
    PilhaAnimada *m_paginas = nullptr;
    QButtonGroup *m_grupoNavegacao = nullptr;
    QVBoxLayout *m_layoutNavegacao = nullptr;
    BotaoNav *m_botaoTema = nullptr;
    QLabel *m_titulo = nullptr;
    QList<BotaoNav *> m_botoesNav;  // para recolorir os ícones quando o tema muda
    QTimer *m_timerBackup = nullptr;
    GerenteDeLembretes *m_lembretes = nullptr;
    AssistentePage *m_paginaAssistente = nullptr;
    AtualizacaoService *m_atualizacao = nullptr;
    QPushButton *m_botaoNovaVersao = nullptr;
    QTimer *m_timerInatividade = nullptr;
    QElapsedTimer m_ultimaAtividade;
    bool m_bloqueadaPorInatividade = false;
    bool m_cursorDeBorda = false;

    // Páginas que recebem navegação vinda da busca global e de outras telas.
    TurmasPage *m_paginaTurmas = nullptr;
    AulasPage *m_paginaAulas = nullptr;
    AnotacoesPage *m_paginaAnotacoes = nullptr;
    TarefasPage *m_paginaTarefas = nullptr;
    CalendarioPage *m_paginaCalendario = nullptr;
};
