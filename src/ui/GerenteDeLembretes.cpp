#include "ui/GerenteDeLembretes.h"

#include <QAction>
#include <QCryptographicHash>
#include <QDate>
#include <QIcon>
#include <QMenu>
#include <QSettings>
#include <QStringList>
#include <QSystemTrayIcon>
#include <QTimer>

#include <utility>

namespace {

constexpr int kIntervaloDaChecagemMs = 30 * 1000;
constexpr int kMaximoDeAvisosSeparados = 3;  // acima disso viram um aviso só (para não "metralhar" a tela)
constexpr int kDuracaoDoAvisoMs = 10 * 1000;

const char *kChaveAtivos = "lembretes/ativos";
const char *kChaveAntecedencia = "lembretes/antecedenciaAula";
const char *kChavePrazos = "lembretes/prazos";
const char *kChaveEnviados = "lembretes/enviados";

}  // namespace

GerenteDeLembretes::GerenteDeLembretes(AgendaRepository &agenda, TarefaRepository &tarefas, const QString &escopo,
                                       QObject *parent)
    : QObject(parent), m_servico(agenda, tarefas)
{
    // O escopo entra só como resumo (não grava e-mail no registro do Windows).
    m_escopo = QString::fromLatin1(QCryptographicHash::hash(escopo.toUtf8(), QCryptographicHash::Sha1).toHex().left(8));
    carregarEnviados();

    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        m_bandeja = new QSystemTrayIcon(QIcon(QStringLiteral(":/icons/icon-256.png")), this);
        m_bandeja->setToolTip(QStringLiteral("Caderno+"));

        m_menu = new QMenu;  // sem pai: o menu da bandeja não pode ser filho de um widget que se esconde
        auto *abrir = m_menu->addAction(QStringLiteral("Abrir o Caderno+"));
        auto *configurar = m_menu->addAction(QStringLiteral("Lembretes…"));
        connect(abrir, &QAction::triggered, this, &GerenteDeLembretes::abrirSolicitado);
        connect(configurar, &QAction::triggered, this, &GerenteDeLembretes::configurarSolicitado);
        m_bandeja->setContextMenu(m_menu);
        connect(m_menu, &QObject::destroyed, this, [this] { m_menu = nullptr; });

        connect(m_bandeja, &QSystemTrayIcon::messageClicked, this, &GerenteDeLembretes::abrirSolicitado);
        connect(m_bandeja, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason motivo) {
            if (motivo == QSystemTrayIcon::Trigger || motivo == QSystemTrayIcon::DoubleClick)
                emit abrirSolicitado();
        });
    }

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &GerenteDeLembretes::verificar);
    m_timer->start(kIntervaloDaChecagemMs);

    if (m_bandeja && configuracao().ativos)
        m_bandeja->show();
    QTimer::singleShot(5000, this, &GerenteDeLembretes::verificar);  // uma conferência logo após abrir
}

LembreteUtil::Config GerenteDeLembretes::configuracao()
{
    QSettings s;
    LembreteUtil::Config c;
    c.ativos = s.value(QLatin1String(kChaveAtivos), c.ativos).toBool();
    const int minutos = s.value(QLatin1String(kChaveAntecedencia), c.antecedenciaAula).toInt();
    c.antecedenciaAula = LembreteUtil::antecedenciaValida(minutos) ? minutos : LembreteUtil::Config().antecedenciaAula;
    c.prazos = s.value(QLatin1String(kChavePrazos), c.prazos).toBool();
    return c;
}

void GerenteDeLembretes::salvarConfiguracao(const LembreteUtil::Config &config)
{
    QSettings s;
    s.setValue(QLatin1String(kChaveAtivos), config.ativos);
    s.setValue(QLatin1String(kChaveAntecedencia), config.antecedenciaAula);
    s.setValue(QLatin1String(kChavePrazos), config.prazos);
}

void GerenteDeLembretes::configuracaoAlterada()
{
    if (m_bandeja)
        m_bandeja->setVisible(configuracao().ativos);
    verificar();
}

void GerenteDeLembretes::notificarTeste()
{
    if (!m_bandeja)
        return;
    m_bandeja->show();  // o aviso de teste também precisa do ícone na bandeja
    m_bandeja->showMessage(QStringLiteral("Lembretes do Caderno+"),
                           QStringLiteral("Está funcionando! Você será avisado das aulas, tarefas e provas."),
                           QSystemTrayIcon::Information, kDuracaoDoAvisoMs);
}

QString GerenteDeLembretes::chaveCompleta(const QString &chave) const
{
    return m_escopo + QLatin1Char(':') + chave;
}

void GerenteDeLembretes::verificar()
{
    const LembreteUtil::Config config = configuracao();
    if (!m_bandeja || !config.ativos)
        return;

    QList<Lembrete> novos;
    for (const Lembrete &l : m_servico.pendentes(QDateTime::currentDateTime(), config)) {
        if (!m_enviados.contains(chaveCompleta(l.chave)))
            novos << l;
    }
    if (novos.isEmpty())
        return;

    for (const Lembrete &l : std::as_const(novos))
        m_enviados.insert(chaveCompleta(l.chave));
    salvarEnviados();
    mostrar(novos);
}

void GerenteDeLembretes::mostrar(const QList<Lembrete> &novos)
{
    // Os textos vêm do usuário (nomes de turma, tarefas): o showMessage os mostra como texto simples.
    if (novos.size() <= kMaximoDeAvisosSeparados) {
        for (const Lembrete &l : novos)
            m_bandeja->showMessage(l.titulo, l.texto, QSystemTrayIcon::Information, kDuracaoDoAvisoMs);
        return;
    }
    QStringList linhas;
    for (int i = 0; i < kMaximoDeAvisosSeparados; ++i)
        linhas << QStringLiteral("%1: %2").arg(novos.at(i).titulo, novos.at(i).texto);
    linhas << QStringLiteral("e mais %1 — veja o painel Hoje.").arg(novos.size() - kMaximoDeAvisosSeparados);
    m_bandeja->showMessage(QStringLiteral("%1 lembretes").arg(novos.size()), linhas.join(QLatin1Char('\n')),
                           QSystemTrayIcon::Information, kDuracaoDoAvisoMs);
}

// Guarda só os avisos de HOJE (a última parte de cada chave é a data do aviso): a lista não cresce para sempre.
void GerenteDeLembretes::carregarEnviados()
{
    const QString hoje = QDate::currentDate().toString(Qt::ISODate);
    m_enviados.clear();
    for (const QString &chave : QSettings().value(QLatin1String(kChaveEnviados)).toStringList()) {
        if (chave.section(QLatin1Char(':'), -1) == hoje)
            m_enviados.insert(chave);
    }
}

void GerenteDeLembretes::salvarEnviados() const
{
    const QString hoje = QDate::currentDate().toString(Qt::ISODate);
    QStringList lista;
    for (const QString &chave : m_enviados) {
        if (chave.section(QLatin1Char(':'), -1) == hoje)
            lista << chave;
    }
    QSettings().setValue(QLatin1String(kChaveEnviados), lista);
}
