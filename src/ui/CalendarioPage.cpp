#include "ui/CalendarioPage.h"
#include "ui/ThemeManager.h"

#include "database/AgendaRepository.h"
#include "database/EventoRepository.h"
#include "database/Repositorios.h"
#include "database/TarefaRepository.h"
#include "database/TurmaRepository.h"
#include "ui/EventoDialog.h"

#include <QBrush>
#include <QCalendarWidget>
#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTextCharFormat>
#include <QVBoxLayout>

namespace {

const QLocale &ptBR()
{
    static const QLocale pt(QLocale::Portuguese, QLocale::Brazil);
    return pt;
}

// Nome e ícone de cada categoria de item do calendário.
QString rotuloDaCategoria(const QString &categoria)
{
    if (categoria == QLatin1String("prova")) return QStringLiteral("Prova");
    if (categoria == QLatin1String("feriado")) return QStringLiteral("Feriado");
    if (categoria == QLatin1String("recesso")) return QStringLiteral("Recesso");
    if (categoria == QLatin1String("reuniao")) return QStringLiteral("Reunião");
    if (categoria == QLatin1String("tarefa")) return QStringLiteral("Tarefa");
    if (categoria == QLatin1String("avaliacao")) return QStringLiteral("Avaliação");
    return QStringLiteral("Evento");
}

QIcon iconeDaCategoria(const QString &categoria)
{
    const char *nome = "pin";
    if (categoria == QLatin1String("prova")) nome = "anotacoes";
    else if (categoria == QLatin1String("feriado")) nome = "feriado";
    else if (categoria == QLatin1String("recesso")) nome = "recesso";
    else if (categoria == QLatin1String("reuniao")) nome = "turmas";
    else if (categoria == QLatin1String("tarefa")) nome = "tarefa-ok";
    else if (categoria == QLatin1String("avaliacao")) nome = "notas";
    return ThemeManager::iconeColorido(QLatin1String(nome), Tokens::Id::InkMuted, 18);
}

bool ehEvento(const QString &categoria)
{
    return categoria != QLatin1String("tarefa") && categoria != QLatin1String("avaliacao");
}

}  // namespace

CalendarioPage::CalendarioPage(Repositorios &repos, QWidget *parent)
    : QWidget(parent),
      m_eventos(repos.eventos),
      m_tarefas(repos.tarefas),
      m_turmas(repos.turmas),
      m_agenda(repos.agenda)
{
    auto *raiz = new QVBoxLayout(this);
    raiz->setContentsMargins(32, 28, 32, 24);
    raiz->setSpacing(6);

    auto *titulo = new QLabel(QStringLiteral("Calendário escolar"));
    titulo->setObjectName(QStringLiteral("pageTitle"));
    auto *subtitulo = new QLabel(QStringLiteral(
        "Provas, feriados, reuniões, prazos de tarefas e avaliações. Dias marcados têm algo acontecendo."));
    subtitulo->setObjectName(QStringLiteral("pageSubtitle"));
    raiz->addWidget(titulo);
    raiz->addWidget(subtitulo);
    raiz->addSpacing(12);

    auto *corpo = new QHBoxLayout;
    corpo->setSpacing(20);

    m_calendario = new QCalendarWidget;
    m_calendario->setLocale(ptBR());
    m_calendario->setGridVisible(true);
    m_calendario->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    m_calendario->setFirstDayOfWeek(Qt::Sunday);
    m_calendario->setMinimumWidth(520);
    corpo->addWidget(m_calendario, 3);

    // Painel do dia selecionado
    auto *painel = new QVBoxLayout;
    m_tituloDia = new QLabel;
    m_tituloDia->setObjectName(QStringLiteral("sectionTitle"));
    painel->addWidget(m_tituloDia);

    m_lista = new QListWidget;
    m_lista->setAlternatingRowColors(true);
    painel->addWidget(m_lista, 1);

    auto *botoes = new QHBoxLayout;
    m_btnNovo = new QPushButton(QStringLiteral("+ Evento"));
    m_btnNovo->setObjectName(QStringLiteral("primary"));
    m_btnEditar = new QPushButton(QStringLiteral("Editar"));
    m_btnExcluir = new QPushButton(QStringLiteral("Excluir"));
    m_btnExcluir->setObjectName(QStringLiteral("danger"));
    botoes->addWidget(m_btnNovo);
    botoes->addStretch(1);
    botoes->addWidget(m_btnEditar);
    botoes->addWidget(m_btnExcluir);
    painel->addLayout(botoes);

    auto *legenda = new QLabel(QStringLiteral("Vermelho: feriado/recesso · Verde: prova · Ocre: outros eventos · Sublinhado: prazo de tarefa"));
    legenda->setObjectName(QStringLiteral("muted"));
    legenda->setWordWrap(true);
    painel->addWidget(legenda);
    corpo->addLayout(painel, 2);
    raiz->addLayout(corpo, 1);

    // As marcações dos dias acompanham o tema.
    connect(&ThemeManager::notificador(), &ThemeNotifier::temaMudou, this, [this] {
        if (isVisible())
            carregarPeriodoVisivel();
    });
    connect(m_calendario, &QCalendarWidget::selectionChanged, this, &CalendarioPage::mostrarDiaSelecionado);
    connect(m_calendario, &QCalendarWidget::currentPageChanged, this, [this] { carregarPeriodoVisivel(); });
    connect(m_btnNovo, &QPushButton::clicked, this, &CalendarioPage::novoEvento);
    connect(m_btnEditar, &QPushButton::clicked, this, &CalendarioPage::editarEvento);
    connect(m_btnExcluir, &QPushButton::clicked, this, &CalendarioPage::excluirEvento);
    connect(m_lista, &QListWidget::itemSelectionChanged, this, &CalendarioPage::atualizarBotoes);
    connect(m_lista, &QListWidget::itemDoubleClicked, this, [this] { abrirItemSelecionado(); });
}

void CalendarioPage::showEvent(QShowEvent *evento)
{
    QWidget::showEvent(evento);
    carregarPeriodoVisivel();
}

void CalendarioPage::irParaData(const QDate &data)
{
    m_calendario->setSelectedDate(data);
    m_calendario->setCurrentPage(data.year(), data.month());
    carregarPeriodoVisivel();
}

// ============================================================================

void CalendarioPage::carregarPeriodoVisivel()
{
    // O calendário mostra também dias do mês anterior/seguinte: busca com folga de 7 dias.
    const QDate primeiro(m_calendario->yearShown(), m_calendario->monthShown(), 1);
    const QDate de = primeiro.addDays(-7);
    const QDate ate = primeiro.addMonths(1).addDays(6);

    m_itens.clear();

    for (const Evento &e : m_eventos.listarPeriodo(de, ate)) {
        QString texto = e.titulo;
        if (!e.turmaNome.isEmpty())
            texto += QStringLiteral(" (%1)").arg(e.turmaNome);
        for (QDate d = qMax(e.dataInicio, de); d <= qMin(e.ultimoDia(), ate); d = d.addDays(1))
            m_itens[d].append({e.tipo, texto, e.id});
    }
    for (const Tarefa &t : m_tarefas.listarComPrazo(de, ate)) {
        QString texto = t.titulo;
        if (t.concluida)
            texto += QStringLiteral(" (concluída)");
        m_itens[t.dataEntrega].append({QStringLiteral("tarefa"), texto, t.id});
    }
    for (const auto &a : m_agenda.avaliacoesDatadas(de, ate))
        m_itens[a.data].append({a.tipo == QLatin1String("prova") ? QStringLiteral("prova") : QStringLiteral("avaliacao"),
                                QStringLiteral("%1 — %2").arg(a.nome, a.turmaNome), 0});

    // Fins de semana em danger (o vermelho padrão do Qt não tem contraste no tema escuro).
    QTextCharFormat fimDeSemana;
    fimDeSemana.setForeground(QBrush(ThemeManager::cor(Tokens::Id::Danger)));
    m_calendario->setWeekdayTextFormat(Qt::Saturday, fimDeSemana);
    m_calendario->setWeekdayTextFormat(Qt::Sunday, fimDeSemana);

    // Marca os dias no calendário (cores semitransparentes funcionam nos dois temas).
    m_calendario->setDateTextFormat(QDate(), QTextCharFormat());  // limpa todas as marcações
    for (auto it = m_itens.constBegin(); it != m_itens.constEnd(); ++it) {
        bool feriado = false, prova = false, evento = false, tarefa = false;
        QStringList dicas;
        for (const ItemDia &item : it.value()) {
            dicas << QStringLiteral("%1: %2").arg(rotuloDaCategoria(item.categoria), item.texto);
            if (item.categoria == QLatin1String("feriado") || item.categoria == QLatin1String("recesso")) feriado = true;
            else if (item.categoria == QLatin1String("prova")) prova = true;
            else if (item.categoria == QLatin1String("tarefa")) tarefa = true;
            else evento = true;
        }
        QTextCharFormat fmt;
        if (feriado)
            fmt.setBackground(QBrush(ThemeManager::comAlfa(Tokens::Id::Danger, 90)));
        else if (prova)
            fmt.setBackground(QBrush(ThemeManager::comAlfa(Tokens::Id::Primary, 90)));
        else if (evento)
            fmt.setBackground(QBrush(ThemeManager::comAlfa(Tokens::Id::Accent, 80)));
        if (feriado || prova || evento) {
            fmt.setFontWeight(QFont::Bold);
            fmt.setForeground(QBrush(ThemeManager::cor(Tokens::Id::Ink)));  // legível sobre o fundo colorido
        }
        if (tarefa) {
            fmt.setFontUnderline(true);
            fmt.setFontWeight(QFont::Bold);
        }
        fmt.setToolTip(dicas.join(QLatin1Char('\n')));
        m_calendario->setDateTextFormat(it.key(), fmt);
    }
    mostrarDiaSelecionado();
}

void CalendarioPage::mostrarDiaSelecionado()
{
    const QDate dia = m_calendario->selectedDate();
    QString titulo = ptBR().toString(dia, QStringLiteral("dddd, d 'de' MMMM"));
    titulo[0] = titulo[0].toUpper();
    m_tituloDia->setText(titulo);

    m_lista->clear();
    m_itensDoDia = m_itens.value(dia);
    if (m_itensDoDia.isEmpty()) {
        auto *vazio = new QListWidgetItem(QStringLiteral("Nada marcado para este dia."));
        vazio->setFlags(Qt::NoItemFlags);
        m_lista->addItem(vazio);
    }
    for (const ItemDia &item : m_itensDoDia)
        m_lista->addItem(new QListWidgetItem(iconeDaCategoria(item.categoria), item.texto));
    atualizarBotoes();
}

const CalendarioPage::ItemDia *CalendarioPage::itemSelecionado() const
{
    const int linha = m_lista->currentRow();
    if (linha < 0 || linha >= m_itensDoDia.size())
        return nullptr;
    return &m_itensDoDia.at(linha);
}

void CalendarioPage::atualizarBotoes()
{
    const ItemDia *item = itemSelecionado();
    const bool eventoSelecionado = item && ehEvento(item->categoria);
    m_btnEditar->setEnabled(eventoSelecionado);
    m_btnExcluir->setEnabled(eventoSelecionado);
}

void CalendarioPage::novoEvento()
{
    Evento base;
    base.dataInicio = m_calendario->selectedDate();
    EventoDialog dlg(m_turmas.listar(false), base, false, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    if (m_eventos.inserir(dlg.evento()) == 0) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_eventos.ultimoErro());
        return;
    }
    carregarPeriodoVisivel();
}

void CalendarioPage::editarEvento()
{
    const ItemDia *item = itemSelecionado();
    if (!item || !ehEvento(item->categoria))
        return;
    const auto existente = m_eventos.buscar(item->id);
    if (!existente)
        return;

    EventoDialog dlg(m_turmas.listar(false), *existente, true, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    if (!m_eventos.atualizar(dlg.evento())) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_eventos.ultimoErro());
        return;
    }
    carregarPeriodoVisivel();
}

void CalendarioPage::excluirEvento()
{
    const ItemDia *item = itemSelecionado();
    if (!item || !ehEvento(item->categoria))
        return;
    const auto evento = m_eventos.buscar(item->id);
    if (!evento)
        return;

    const auto resp = QMessageBox::question(this, QStringLiteral("Excluir evento"),
                                            QStringLiteral("Excluir o evento \"%1\"%2?")
                                                .arg(evento->titulo,
                                                     evento->dataFim.isValid() ? QStringLiteral(" (todos os dias dele)") : QString()),
                                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (resp != QMessageBox::Yes)
        return;
    if (!m_eventos.remover(evento->id)) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_eventos.ultimoErro());
        return;
    }
    carregarPeriodoVisivel();
}

void CalendarioPage::abrirItemSelecionado()
{
    const ItemDia *item = itemSelecionado();
    if (!item)
        return;
    if (item->categoria == QLatin1String("tarefa"))
        emit abrirTarefaSolicitada(item->id);
    else if (ehEvento(item->categoria))
        editarEvento();
}
