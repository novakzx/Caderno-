#pragma once

#include "core/Tokens.h"

#include <QColor>
#include <QIcon>
#include <QObject>
#include <QPixmap>
#include <QString>

class QAbstractButton;
class QWidget;

// Avisa as telas quando o tema claro/escuro muda (para refazerem o que guardam com cor).
class ThemeNotifier : public QObject {
    Q_OBJECT
signals:
    void temaMudou();
};

// Gerencia o tema claro/escuro do design system do Caderno+ (design/tokens.json).
// A escolha é salva em QSettings e reaplicada na próxima abertura do programa.
//
// Cores: o resto do programa NUNCA escreve hex; pede pelo nome do token
// (ThemeManager::cor(Tokens::Id::Danger)) e recebe a cor do tema atual.
class ThemeManager {
public:
    enum class Tema { Claro, Escuro };

    // Estado de um aviso/mensagem (cor vem do QSS, via propriedade "estado").
    enum class Estado { Neutro, Sucesso, Aviso, Erro };

    // Registra as fontes Figtree (embutidas no .exe). Chamar uma vez, depois de criar o QApplication.
    static void carregarFontes();

    // Lê o tema salvo (padrão: claro) e aplica no QApplication.
    static void carregarSalvo();
    static void aplicar(Tema tema);
    static Tema atual();
    static void alternar();
    static ThemeNotifier &notificador();

    // --- Cores ---
    static QColor cor(Tokens::Id id);              // no tema atual
    static QColor cor(Tokens::Id id, Tema tema);
    static QString corHex(Tokens::Id id);          // "#rrggbb" do tema atual, para QSS/HTML
    static QColor comAlfa(Tokens::Id id, int alfa);  // token com transparência (0..255)

    // Cor de uma turma. O banco guarda o nome do token ("turma-3"), que acompanha o tema;
    // turmas antigas com cor escolhida à mão ("#rrggbb") continuam com essa cor fixa.
    static QColor corDaTurma(const QString &salva);
    static QString corDaTurmaHex(const QString &salva);
    // Texto legível sobre a cor da turma (on-turma para os tokens; automático para cor manual).
    static QColor textoSobreTurma(const QString &salva);

    // Cor de uma marca da chamada: "P", "F", "J" ou "A".
    static QColor corDaSituacao(const QString &situacao);
    static QColor fundoDaSituacao(const QString &situacao);  // só o "A" (ocre suave); demais: inválida

    // --- Ícones SVG de traço fino (resources/icons/<nome>.svg), recoloridos pelo tema ---
    // Normal: ink-muted; quando o botão está marcado (checked): primary.
    static QIcon icone(const QString &nome, int tamanho = 20);
    static QIcon iconeColorido(const QString &nome, Tokens::Id token, int tamanho = 16);
    static QIcon iconeColorido(const QString &nome, const QColor &cor, int tamanho = 16);
    // Desenha o ícone numa cor (nítido em telas de alta densidade).
    static QPixmap pixmap(const QString &nome, const QColor &cor, int tamanho);
    // Põe um ícone num botão e o mantém na cor certa quando o tema muda
    // (use OnPrimary em botões "primary").
    static void iconeNoBotao(QAbstractButton *botao, const QString &nome, Tokens::Id cor = Tokens::Id::Ink,
                             int tamanho = 16);

    // Marca um QLabel/QWidget com um estado (a cor vem do QSS e muda junto com o tema).
    static void definirEstado(QWidget *widget, Estado estado);

private:
    static QString montarFolhaDeEstilo(Tema tema);
    static QString prepararImagensDoQss(Tema tema);
    static void aplicarFonte();
};
