#include "ui/HojePage.h"
#include "ui/ThemeManager.h"

#include "core/AtencaoUtil.h"
#include "database/AgendaRepository.h"
#include "database/Repositorios.h"
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
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int kDiasProvas = 14;      // janela de "provas próximas"
constexpr int kMaximoDeAlunos = 6;   // quantos alunos o cartão "em atenção" mostra de uma vez

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
QFrame *criarCartao(const QString &icone, const QString &titulo, QVBoxLayout **lista)
{
    auto *cartao = new QFrame;
    cartao->setObjectName(QStringLiteral("card"));
    auto *layout = new QVBoxLayout(cartao);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);

    // Título do cartão: ícone (refeito quando o tema muda) + texto.
    auto *cabecalho = new QHBoxLayout;
    cabecalho->setSpacing(8);
    auto *lblIcone = new QLabel;
    lblIcone->setProperty("iconeDoCartao", icone);
    lblIcone->setPixmap(ThemeManager::pixmap(icone, ThemeManager::cor(Tokens::Id::InkMuted), 20));
    lblIcone->setFixedSize(22, 22);
    auto *lblTitulo = new QLabel(titulo);
    lblTitulo->setObjectName(QStringLiteral("sectionTitle"));
    cabecalho->addWidget(lblIcone);
    cabecalho->addWidget(lblTitulo, 1);
    layout->addLayout(cabecalho);

    *lista = new QVBoxLayout;
    (*lista)->setSpacing(8);
    layout->addLayout(*lista);
    layout->addStretch(1);
    return cartao;
}

// Indicador do topo do painel: ícone + rótulo e, embaixo, o número em destaque.
QFrame *criarIndicador(const QString &icone, const QString &rotulo, QLabel **numero)
{
    auto *quadro = new QFrame;
    quadro->setObjectName(QStringLiteral("indicador"));
    auto *layout = new QVBoxLayout(quadro);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(4);

    auto *cabecalho = new QHBoxLayout;
    cabecalho->setSpacing(8);
    auto *lblIcone = new QLabel;
    lblIcone->setProperty("iconeDoCartao", icone);  // refeito quando o tema muda
    lblIcone->setPixmap(ThemeManager::pixmap(icone, ThemeManager::cor(Tokens::Id::InkMuted), 20));
    lblIcone->setFixedSize(22, 22);
    auto *lblRotulo = new QLabel(rotulo);
    lblRotulo->setObjectName(QStringLiteral("indicadorRotulo"));
    cabecalho->addWidget(lblIcone);
    cabecalho->addWidget(lblRotulo, 1);
    layout->addLayout(cabecalho);

    *numero = new QLabel(QStringLiteral("0"));
    (*numero)->setObjectName(QStringLiteral("indicadorNumero"));
    layout->addWidget(*numero);
    return quadro;
}

QLabel *textoMudo(const QString &texto)
{
    auto *l = new QLabel(texto);
    l->setTextFormat(Qt::PlainText);  // pode ter nome de turma/tarefa: nunca como HTML
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

// 5,9 · 74,96 · 80 (duas casas, sem zeros sobrando)
QString numeroCurto(double v)
{
    QString s = QString::number(v, 'f', 2);
    while (s.endsWith(QLatin1Char('0')))
        s.chop(1);
    if (s.endsWith(QLatin1Char('.')))
        s.chop(1);
    return s.replace(QLatin1Char('.'), QLatin1Char(','));
}

// "Média abaixo da nota de corte (5,5) · Frequência abaixo de 75% (70%)"
QString textoDosMotivos(const AlunoEmAtencao &a)
{
    QStringList partes;
    for (const AtencaoUtil::Motivo m : a.motivos) {
        QString texto = QString::fromUtf8(AtencaoUtil::rotulo(m));
        switch (m) {
        case AtencaoUtil::Motivo::MediaAbaixo:
        case AtencaoUtil::Motivo::MediaPerto:
            if (a.media)
                texto += QStringLiteral(" (%1)").arg(numeroCurto(*a.media));
            break;
        case AtencaoUtil::Motivo::FrequenciaAbaixo:
        case AtencaoUtil::Motivo::FrequenciaPerto:
            if (a.frequenciaPct)
                texto += QStringLiteral(" (%1%)").arg(numeroCurto(*a.frequenciaPct));
            break;
        case AtencaoUtil::Motivo::Ocorrencias:
            texto += QStringLiteral(" (%1 em %2 dias)").arg(a.ocorrenciasNegativas).arg(AtencaoUtil::Limites().diasDeOcorrencias);
            break;
        }
        partes << texto;
    }
    return partes.join(QStringLiteral(" · "));
}

}  // namespace

HojePage::HojePage(Repositorios &repos, QWidget *parent)
    : QWidget(parent), m_agenda(repos.agenda), m_tarefas(repos.tarefas), m_desempenho(repos)
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
    raiz->addSpacing(10);

    // Faixa de indicadores (os números também estão escritos nos cartões abaixo).
    auto *faixa = new QHBoxLayout;
    faixa->setSpacing(16);
    faixa->addWidget(criarIndicador(QStringLiteral("aulas"), QStringLiteral("Aulas hoje"), &m_numAulas));
    faixa->addWidget(criarIndicador(QStringLiteral("tarefa-ok"), QStringLiteral("Tarefas pendentes"), &m_numTarefas));
    faixa->addWidget(criarIndicador(QStringLiteral("anotacoes"),
                                    QStringLiteral("Provas em %1 dias").arg(kDiasProvas), &m_numProvas));
    faixa->addWidget(criarIndicador(QStringLiteral("alerta"), QStringLiteral("Alunos em atenção"), &m_numAtencao));
    raiz->addLayout(faixa);
    raiz->addSpacing(10);

    // Quatro cartões: aulas | tarefas  /  alunos em atenção | provas  (coluna da esquerda mais larga)
    auto *grade = new QGridLayout;
    grade->setHorizontalSpacing(16);
    grade->setVerticalSpacing(16);
    grade->addWidget(criarCartao(QStringLiteral("aulas"), QStringLiteral("Aulas de hoje"), &m_listaAulas), 0, 0);

    QFrame *cartaoTarefas = criarCartao(QStringLiteral("tarefa-ok"), QStringLiteral("Tarefas pendentes"), &m_listaTarefas);
    m_novaTarefa = new QLineEdit;
    m_novaTarefa->setPlaceholderText(QStringLiteral("Nova tarefa… (Enter para adicionar)"));
    m_novaTarefa->setClearButtonEnabled(true);
    cartaoTarefas->layout()->addWidget(m_novaTarefa);
    grade->addWidget(cartaoTarefas, 0, 1);

    grade->addWidget(criarCartao(QStringLiteral("alerta"), QStringLiteral("Alunos em atenção"), &m_listaAtencao), 1, 0);
    grade->addWidget(criarCartao(QStringLiteral("anotacoes"), QStringLiteral("Provas próximas"), &m_listaProvas), 1, 1);
    grade->setColumnStretch(0, 3);
    grade->setColumnStretch(1, 2);
    raiz->addLayout(grade, 1);

    connect(m_novaTarefa, &QLineEdit::returnPressed, this, &HojePage::criarTarefaRapida);
    // Faixas das turmas, provas e prazos atrasados acompanham o tema.
    connect(&ThemeManager::notificador(), &ThemeNotifier::temaMudou, this, [this] {
        for (QLabel *rotulo : findChildren<QLabel *>()) {  // ícones dos títulos dos cartões
            const QVariant nome = rotulo->property("iconeDoCartao");
            if (nome.isValid())
                rotulo->setPixmap(ThemeManager::pixmap(nome.toString(), ThemeManager::cor(Tokens::Id::InkMuted), 20));
        }
        if (isVisible())
            atualizar();
    });

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

    int aulas = 0, tarefas = 0, provas = 0, atencao = 0;
    atualizarAulas(hoje, agora, &aulas);
    atualizarTarefas(hoje, &tarefas);
    atualizarProvas(hoje, &provas);
    atualizarAtencao(&atencao);

    m_numAulas->setText(QString::number(aulas));
    m_numTarefas->setText(QString::number(tarefas));
    m_numProvas->setText(QString::number(provas));
    m_numAtencao->setText(QString::number(atencao));
    m_resumo->setText(aulas == 0 && tarefas == 0 && atencao == 0
                          ? QStringLiteral("Dia tranquilo: nada marcado para hoje.")
                          : QStringLiteral("O que pede a sua atenção hoje."));
}

// ============================================================================

void HojePage::atualizarAulas(const QDate &hoje, const QTime &agora, int *total)
{
    esvaziar(m_listaAulas);
    const auto aulas = m_agenda.aulasDoDia(hoje);
    *total = aulas.size();

    if (aulas.isEmpty()) {
        m_listaAulas->addWidget(textoMudo(QStringLiteral("Nenhuma aula hoje.")));
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
        linha->setObjectName(QStringLiteral("linhaDoCartao"));
        auto *hl = new QHBoxLayout(linha);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(12);

        // Faixa colorida da turma
        auto *faixa = new QFrame;
        faixa->setFixedWidth(6);
        faixa->setStyleSheet(QStringLiteral("background: %1; border-radius: 3px;").arg(ThemeManager::corDaTurmaHex(h.turmaCor)));
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
        m_listaTarefas->addWidget(textoMudo(QStringLiteral("Nada pendente. Bom trabalho!")));
        return;
    }

    for (const Tarefa &t : tarefas) {
        auto *linha = new QWidget;
        linha->setObjectName(QStringLiteral("linhaDoCartao"));
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
                corDetalhe = ThemeManager::corHex(Tokens::Id::Danger);
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
                           "<span style='color:%6'>%4 (%5)</span>")
                .arg(ThemeManager::corDaTurmaHex(p.turmaCor), p.titulo.toHtmlEscaped(),
                     p.turmaNome.isEmpty() ? QStringLiteral("geral") : p.turmaNome.toHtmlEscaped(),
                     ptBR().toString(p.data, QStringLiteral("ddd, dd/MM")), textoRelativo(dias),
                     ThemeManager::corHex(Tokens::Id::InkMuted)));
        linha->setTextFormat(Qt::RichText);
        linha->setWordWrap(true);
        m_listaProvas->addWidget(linha);
    }
}

void HojePage::atualizarAtencao(int *total)
{
    esvaziar(m_listaAtencao);
    // A nota de corte é a mesma da tela de Notas (o professor a ajusta lá).
    const double corte = QSettings().value(QStringLiteral("notaCorte"), 6.0).toDouble();
    const QList<AlunoEmAtencao> alunos = m_desempenho.alunosEmAtencao(corte, QDate::currentDate());
    *total = alunos.size();

    if (alunos.isEmpty()) {
        m_listaAtencao->addWidget(textoMudo(QStringLiteral("Nenhum aluno em atenção. Médias e frequência estão em dia.")));
        return;
    }

    const int mostrar = qMin<int>(alunos.size(), kMaximoDeAlunos);
    for (int i = 0; i < mostrar; ++i) {
        const AlunoEmAtencao &a = alunos.at(i);
        const bool critico = a.nivel == AtencaoUtil::Nivel::Critico;

        auto *linha = new QWidget;
        linha->setObjectName(QStringLiteral("linhaDoCartao"));
        auto *vl = new QVBoxLayout(linha);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(1);

        auto *topo = new QHBoxLayout;
        topo->setSpacing(8);
        // "&" num botão vira atalho de teclado: nomes como "Ana & Bia" precisam dele dobrado.
        auto *botao = new QPushButton(QString(a.nome).replace(QLatin1Char('&'), QStringLiteral("&&")));
        botao->setObjectName(QStringLiteral("link"));
        botao->setCursor(Qt::PointingHandCursor);
        botao->setToolTip(QStringLiteral("Abrir %1 na turma").arg(a.turmaNome));
        const int turmaId = a.turmaId;
        const int alunoId = a.alunoId;
        connect(botao, &QPushButton::clicked, this, [this, turmaId, alunoId] { emit abrirAlunoSolicitado(turmaId, alunoId); });
        topo->addWidget(botao);
        topo->addWidget(textoMudo(a.turmaNome));
        topo->addStretch(1);
        // O nível é dito em palavras (e a cor só reforça).
        auto *selo = new QLabel(critico ? QStringLiteral("Urgente") : QStringLiteral("Atenção"));
        selo->setObjectName(critico ? QStringLiteral("seloCritico") : QStringLiteral("seloAtencao"));
        topo->addWidget(selo, 0, Qt::AlignVCenter);
        vl->addLayout(topo);

        auto *detalhe = textoMudo(textoDosMotivos(a));
        detalhe->setContentsMargins(6, 0, 0, 0);
        vl->addWidget(detalhe);
        m_listaAtencao->addWidget(linha);
    }
    if (alunos.size() > mostrar)
        m_listaAtencao->addWidget(textoMudo(QStringLiteral("e mais %1 aluno(s)…").arg(alunos.size() - mostrar)));
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
