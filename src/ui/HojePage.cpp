#include "ui/HojePage.h"

#include "database/AgendaRepository.h"
#include "database/TarefaRepository.h"

#include <QCheckBox>
#include <QDate>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int kDiasProvas = 14;  // janela de "provas próximas"

const QLocale &ptBR()
{
    static const QLocale pt(QLocale::Portuguese, QLocale::Brazil);
    return pt;
}

QString primeiraMaiuscula(QString s)
{
    if (!s.isEmpty())
        s[0] = s[0].toUpper();
    return s;
}

void esvaziar(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }
}

// Cartão com título e uma lista que o chamador preenche.
QFrame *criarCartao(const QString &titulo, QVBoxLayout **lista)
{
    auto *cartao = new QFrame;
    cartao->setObjectName(QStringLiteral("card"));
    auto *layout = new QVBoxLayout(cartao);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);

    auto *lblTitulo = new QLabel(titulo);
    lblTitulo->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(lblTitulo);

    *lista = new QVBoxLayout;
    (*lista)->setSpacing(8);
    layout->addLayout(*lista);
    layout->addStretch(1);
    return cartao;
}

QLabel *textoMudo(const QString &texto)
{
    auto *l = new QLabel(texto);
    l->setObjectName(QStringLiteral("muted"));
    l->setWordWrap(true);
    return l;
}

// "hoje", "amanhã", "em 5 dias", "atrasada há 2 dias"
QString textoRelativo(int dias)
{
    if (dias == 0)
        return QStringLiteral("hoje");
    if (dias == 1)
        return QStringLiteral("amanhã");
    if (dias > 1)
        return QStringLiteral("em %1 dias").arg(dias);
    if (dias == -1)
        return QStringLiteral("atrasada desde ontem");
    return QStringLiteral("atrasada há %1 dias").arg(-dias);
}

}  // namespace

HojePage::HojePage(AgendaRepository &agenda, TarefaRepository &tarefas, QWidget *parent)
    : QWidget(parent), m_agenda(agenda), m_tarefas(tarefas)
{
    auto *externo = new QVBoxLayout(this);
    externo->setContentsMargins(0, 0, 0, 0);

    auto *rolagem = new QScrollArea;
    rolagem->setWidgetResizable(true);
    rolagem->setFrameShape(QFrame::NoFrame);
    externo->addWidget(rolagem);

    auto *conteudo = new QWidget;
    rolagem->setWidget(conteudo);
    auto *raiz = new QVBoxLayout(conteudo);
    raiz->setContentsMargins(32, 28, 32, 24);
    raiz->setSpacing(6);

    m_data = new QLabel;
    m_data->setObjectName(QStringLiteral("pageTitle"));
    m_resumo = new QLabel;
    m_resumo->setObjectName(QStringLiteral("pageSubtitle"));
    raiz->addWidget(m_data);
    raiz->addWidget(m_resumo);
    raiz->addSpacing(14);

    // Três cartões: aulas (esquerda, mais largo) | tarefas e provas (direita)
    auto *grade = new QGridLayout;
    grade->setHorizontalSpacing(16);
    grade->setVerticalSpacing(16);
    grade->addWidget(criarCartao(QStringLiteral("📚  Aulas de hoje"), &m_listaAulas), 0, 0, 2, 1);

    QFrame *cartaoTarefas = criarCartao(QStringLiteral("✅  Tarefas pendentes"), &m_listaTarefas);
    m_novaTarefa = new QLineEdit;
    m_novaTarefa->setPlaceholderText(QStringLiteral("Nova tarefa… (Enter para adicionar)"));
    m_novaTarefa->setClearButtonEnabled(true);
    cartaoTarefas->layout()->addWidget(m_novaTarefa);
    grade->addWidget(cartaoTarefas, 0, 1);

    grade->addWidget(criarCartao(QStringLiteral("📝  Provas próximas"), &m_listaProvas), 1, 1);
    grade->setColumnStretch(0, 3);
    grade->setColumnStretch(1, 2);
    raiz->addLayout(grade, 1);

    connect(m_novaTarefa, &QLineEdit::returnPressed, this, &HojePage::criarTarefaRapida);

    // Atualiza sozinho (a aula "em andamento" muda com o relógio).
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &HojePage::atualizar);
    m_timer->start(60 * 1000);
}

void HojePage::showEvent(QShowEvent *evento)
{
    QWidget::showEvent(evento);
    atualizar();
}

void HojePage::atualizar()
{
    const QDate hoje = QDate::currentDate();
    const QTime agora = QTime::currentTime();

    m_data->setText(primeiraMaiuscula(ptBR().toString(hoje, QStringLiteral("dddd, d 'de' MMMM"))));

    int aulas = 0, tarefas = 0, provas = 0;
    atualizarAulas(hoje, agora, &aulas);
    atualizarTarefas(hoje, &tarefas);
    atualizarProvas(hoje, &provas);

    m_resumo->setText(QStringLiteral("%1 · %2 · %3")
                          .arg(aulas == 1 ? QStringLiteral("1 aula") : QStringLiteral("%1 aulas").arg(aulas),
                               tarefas == 1 ? QStringLiteral("1 tarefa pendente")
                                            : QStringLiteral("%1 tarefas pendentes").arg(tarefas),
                               provas == 1 ? QStringLiteral("1 prova nos próximos %1 dias").arg(kDiasProvas)
                                           : QStringLiteral("%1 provas nos próximos %2 dias").arg(provas).arg(kDiasProvas)));
}

// ============================================================================

void HojePage::atualizarAulas(const QDate &hoje, const QTime &agora, int *total)
{
    esvaziar(m_listaAulas);
    const auto aulas = m_agenda.aulasDoDia(hoje);
    *total = aulas.size();

    if (aulas.isEmpty()) {
        m_listaAulas->addWidget(textoMudo(QStringLiteral("Nenhuma aula hoje. 🎉")));
        return;
    }

    bool proximaMarcada = false;
    for (const auto &a : aulas) {
        const Horario &h = a.horario;
        const bool emAndamento = h.inicio <= agora && agora < h.fim;
        const bool encerrada = h.fim <= agora;
        const bool ehProxima = !emAndamento && !encerrada && !proximaMarcada;
        if (ehProxima)
            proximaMarcada = true;

        auto *linha = new QWidget;
        auto *hl = new QHBoxLayout(linha);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(12);

        // Faixa colorida da turma
        auto *faixa = new QFrame;
        faixa->setFixedWidth(6);
        faixa->setStyleSheet(QStringLiteral("background: %1; border-radius: 3px;").arg(h.turmaCor));
        hl->addWidget(faixa);

        auto *textos = new QVBoxLayout;
        textos->setSpacing(2);
        QString nome = h.turmaNome;
        if (!h.turmaDisciplina.isEmpty())
            nome += QStringLiteral(" — ") + h.turmaDisciplina;
        auto *lblPrincipal = new QLabel(QStringLiteral("<b>%1 – %2</b>  %3")
                                            .arg(h.inicio.toString(QStringLiteral("HH:mm")),
                                                 h.fim.toString(QStringLiteral("HH:mm")), nome.toHtmlEscaped()));
        lblPrincipal->setTextFormat(Qt::RichText);
        textos->addWidget(lblPrincipal);

        QStringList detalhes;
        if (!h.sala.isEmpty())
            detalhes << QStringLiteral("Sala %1").arg(h.sala);
        if (!a.tema.isEmpty())
            detalhes << QStringLiteral("Tema: %1").arg(a.tema);
        if (!detalhes.isEmpty())
            textos->addWidget(textoMudo(detalhes.join(QStringLiteral(" · "))));
        hl->addLayout(textos, 1);

        if (emAndamento || ehProxima) {
            auto *selo = new QLabel(emAndamento ? QStringLiteral("AGORA") : QStringLiteral("PRÓXIMA"));
            selo->setObjectName(QStringLiteral("badge"));
            hl->addWidget(selo, 0, Qt::AlignVCenter);
        } else if (encerrada) {
            lblPrincipal->setObjectName(QStringLiteral("muted"));  // aulas que já passaram ficam discretas
        }
        m_listaAulas->addWidget(linha);
    }
}

void HojePage::atualizarTarefas(const QDate &hoje, int *total)
{
    esvaziar(m_listaTarefas);
    const auto tarefas = m_tarefas.listarPendentes(15);
    *total = tarefas.size();

    if (tarefas.isEmpty()) {
        m_listaTarefas->addWidget(textoMudo(QStringLiteral("Nada pendente. Bom trabalho! ✨")));
        return;
    }

    for (const Tarefa &t : tarefas) {
        auto *linha = new QWidget;
        auto *vl = new QVBoxLayout(linha);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);

        auto *caixa = new QCheckBox(t.titulo);
        vl->addWidget(caixa);

        QStringList detalhes;
        QString corDetalhe;
        if (t.dataEntrega.isValid()) {
            const int dias = hoje.daysTo(t.dataEntrega);
            detalhes << textoRelativo(dias);
            if (dias < 0)
                corDetalhe = QStringLiteral("#D64545");
        }
        if (!t.turmaNome.isEmpty())
            detalhes << t.turmaNome;
        if (!detalhes.isEmpty()) {
            auto *lbl = textoMudo(detalhes.join(QStringLiteral(" · ")));
            lbl->setContentsMargins(26, 0, 0, 0);  // alinha com o texto da caixa
            if (!corDetalhe.isEmpty())
                lbl->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(corDetalhe));
            vl->addWidget(lbl);
        }

        // Marcar como feita: grava e recarrega a lista. O recarregamento é adiado
        // (singleShot) porque ele destrói esta própria caixa de seleção.
        const int id = t.id;
        connect(caixa, &QCheckBox::toggled, this, [this, id](bool marcada) {
            if (!marcada)
                return;
            if (!m_tarefas.marcarConcluida(id, true))
                QMessageBox::critical(this, QStringLiteral("Erro"), m_tarefas.ultimoErro());
            QTimer::singleShot(0, this, &HojePage::atualizar);
        });
        m_listaTarefas->addWidget(linha);
    }
}

void HojePage::atualizarProvas(const QDate &hoje, int *total)
{
    esvaziar(m_listaProvas);
    const auto provas = m_agenda.provasProximas(hoje, kDiasProvas);
    *total = provas.size();

    if (provas.isEmpty()) {
        m_listaProvas->addWidget(textoMudo(QStringLiteral("Nenhuma prova nos próximos %1 dias.").arg(kDiasProvas)));
        return;
    }

    for (const auto &p : provas) {
        const int dias = hoje.daysTo(p.data);
        auto *linha = new QLabel(
            QStringLiteral("<span style='color:%1'>■</span> <b>%2</b> · %3<br>"
                           "<span style='color:gray'>%4 (%5)</span>")
                .arg(p.turmaCor, p.titulo.toHtmlEscaped(),
                     p.turmaNome.isEmpty() ? QStringLiteral("geral") : p.turmaNome.toHtmlEscaped(),
                     ptBR().toString(p.data, QStringLiteral("ddd, dd/MM")), textoRelativo(dias)));
        linha->setTextFormat(Qt::RichText);
        linha->setWordWrap(true);
        m_listaProvas->addWidget(linha);
    }
}

void HojePage::criarTarefaRapida()
{
    const QString titulo = m_novaTarefa->text().trimmed();
    if (titulo.isEmpty())
        return;

    Tarefa t;
    t.titulo = titulo;
    if (m_tarefas.inserir(t) == 0) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_tarefas.ultimoErro());
        return;
    }
    m_novaTarefa->clear();
    atualizar();
}
