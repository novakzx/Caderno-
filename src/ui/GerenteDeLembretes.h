#pragma once

#include "core/LembreteUtil.h"
#include "services/LembreteService.h"

#include <QObject>
#include <QSet>
#include <QString>

class AgendaRepository;
class QMenu;
class QSystemTrayIcon;
class QTimer;
class TarefaRepository;

// Mostra os lembretes como notificações do Windows (pela área de notificação, ao lado do relógio).
// Confere a agenda a cada 30 segundos enquanto o Caderno+ está aberto; cada aviso aparece uma só vez
// por dia (o que já foi avisado fica guardado). As preferências ficam em QSettings ("lembretes/...").
class GerenteDeLembretes : public QObject {
    Q_OBJECT
public:
    // `escopo` separa as contas: avisos de uma conta não escondem os de outra (os ids se repetem entre bancos).
    GerenteDeLembretes(AgendaRepository &agenda, TarefaRepository &tarefas, const QString &escopo,
                       QObject *parent = nullptr);

    static LembreteUtil::Config configuracao();
    static void salvarConfiguracao(const LembreteUtil::Config &config);

    // Este computador tem área de notificação? (Em sessões remotas/sem interface, não.)
    bool bandejaDisponivel() const { return m_bandeja != nullptr; }

    // Reaplica as preferências (mostra ou esconde o ícone) e confere já os avisos.
    void configuracaoAlterada();
    void notificarTeste();

signals:
    void abrirSolicitado();       // clique na notificação ou no ícone: trazer a janela para a frente
    void configurarSolicitado();  // menu do ícone: "Lembretes…"

private:
    void verificar();
    void mostrar(const QList<Lembrete> &novos);
    QString chaveCompleta(const QString &chave) const;
    void carregarEnviados();
    void salvarEnviados() const;

    LembreteService m_servico;
    QString m_escopo;
    QSystemTrayIcon *m_bandeja = nullptr;
    QMenu *m_menu = nullptr;
    QTimer *m_timer = nullptr;
    QSet<QString> m_enviados;
};
