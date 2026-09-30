#pragma once

#include "models/Horario.h"

#include <QList>
#include <QWidget>

// Grade semanal desenhada à mão: dias nas colunas, horas nas linhas, e cada
// aula é um bloco colorido (cor da turma).
//
// Interação:
//   - arrastar um bloco: move de dia/horário (encaixa de 5 em 5 minutos);
//   - arrastar a borda inferior do bloco: muda a duração;
//   - duplo clique no bloco: editar; duplo clique no vazio: nova aula ali;
//   - botão direito: menu (editar, excluir, nova aula).
//
// O widget só desenha e emite sinais; quem valida e grava no banco é a
// HorarioPage (nenhum acesso a banco aqui).
class HorarioGridWidget : public QWidget {
    Q_OBJECT
public:
    explicit HorarioGridWidget(QWidget *parent = nullptr);

    // diasVisiveis: 5 (seg-sex), 6 (até sábado) ou 7. horaInicio/horaFim: janela
    // de horas exibida (0..23).
    void setDados(const QList<Horario> &aulas, int diasVisiveis, int horaInicio, int horaFim);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    // Usuário soltou um bloco em outro dia/horário (ou mudou a duração).
    void aulaAlterada(int id, int diaSemana, QTime inicio, QTime fim);
    // Soltou em cima de outra aula: nada foi alterado.
    void movimentoRecusado();
    void editarSolicitado(int id);
    void excluirSolicitado(int id);
    void novaAulaSolicitada(int diaSemana, QTime inicio);

protected:
    void paintEvent(QPaintEvent *evento) override;
    void mousePressEvent(QMouseEvent *evento) override;
    void mouseMoveEvent(QMouseEvent *evento) override;
    void mouseReleaseEvent(QMouseEvent *evento) override;
    void mouseDoubleClickEvent(QMouseEvent *evento) override;
    void contextMenuEvent(QContextMenuEvent *evento) override;
    void changeEvent(QEvent *evento) override;

private:
    enum class Modo { Nada, Mover, Redimensionar };

    int larguraColuna() const;
    int yDoMinuto(int minutos) const;
    int minutosNaPos(int y) const;
    int diaNaPos(int x) const;  // 1..m_dias (limitado às colunas existentes)
    QRect retangulo(int dia, int iniMin, int fimMin) const;
    const Horario *blocoEm(const QPoint &pos) const;
    bool bordaInferior(const Horario &aula, const QPoint &pos) const;
    bool haSobreposicao(int dia, int iniMin, int fimMin, int ignorarId) const;
    void cancelarArraste();

    QList<Horario> m_aulas;
    int m_dias = 5;
    int m_horaIni = 7;
    int m_horaFim = 18;

    // Estado do arraste
    Modo m_modo = Modo::Nada;
    bool m_arrastando = false;
    QPoint m_pontoPressionado;
    Horario m_origem;         // cópia do bloco no início do arraste
    int m_deslocamentoMin = 0;  // onde, dentro do bloco, o usuário o agarrou (minutos)
    int m_fantasmaDia = 0;
    int m_fantasmaIni = 0;
    int m_fantasmaFim = 0;
    bool m_fantasmaConflito = false;
};
