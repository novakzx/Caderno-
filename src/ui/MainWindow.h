#pragma once

#include "database/BuscaRepository.h"

#include <QMainWindow>

class AnotacoesPage;
class AulasPage;
class CalendarioPage;
class QButtonGroup;
class QPushButton;
class QStackedWidget;
class QTimer;
class QVBoxLayout;
class TarefasPage;
class TurmasPage;
struct Repositorios;

// Janela principal: barra lateral à esquerda (navegação, busca, backup e tema) e
// área de conteúdo à direita (QStackedWidget com uma página por seção).
//
// Para adicionar uma nova seção, basta chamar adicionarSecao() no construtor
// com o título do botão e a página.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(Repositorios &repos, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *evento) override;

private:
    void construirBarraLateral(QWidget *barra);
    void adicionarSecao(const QString &titulo, QWidget *pagina);
    void atualizarTextoBotaoTema();
    void irParaSecao(int indice);
    void irParaPagina(QWidget *pagina);

    void abrirBusca();
    void navegarPara(const ItemBusca &item);
    void abrirBackup();
    void verificarBackupAutomatico(int intervaloHoras);

    Repositorios &m_repos;

    QStackedWidget *m_paginas = nullptr;
    QButtonGroup *m_grupoNavegacao = nullptr;
    QVBoxLayout *m_layoutNavegacao = nullptr;
    QPushButton *m_botaoTema = nullptr;
    QTimer *m_timerBackup = nullptr;

    // Páginas que recebem navegação vinda da busca global e de outras telas.
    TurmasPage *m_paginaTurmas = nullptr;
    AulasPage *m_paginaAulas = nullptr;
    AnotacoesPage *m_paginaAnotacoes = nullptr;
    TarefasPage *m_paginaTarefas = nullptr;
    CalendarioPage *m_paginaCalendario = nullptr;
};
