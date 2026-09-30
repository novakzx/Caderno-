#include "ui/FrequenciaPage.h"
#include "ui/ThemeManager.h"

#include "core/FrequenciaUtil.h"
#include "database/AlunoRepository.h"
#include "database/EventoRepository.h"
#include "database/FrequenciaRepository.h"
#include "database/Repositorios.h"
#include "database/TurmaRepository.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDateEdit>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

const QLocale &ptBR()
{
    static const QLocale pt(QLocale::Portuguese, QLocale::Brazil);
    return pt;
}

}  // namespace

FrequenciaPage::FrequenciaPage(Repositorios &repos, QWidget *parent)
    : QWidget(parent),
      m_turmas(repos.turmas),
      m_alunos(repos.alunos),
      m_frequencia(repos.frequencia),
      m_eventos(repos.eventos)
{
    auto *raiz = new QVBoxLayout(this);
    raiz->setContentsMargins(32, 28, 32, 24);
    raiz->setSpacing(8);

    auto *titulo = new QLabel(QStringLiteral("Frequência"));
    titulo->setObjectName(QStringLiteral("pageTitle"));
    auto *subtitulo = new QLabel(QStringLiteral(
        "Faça a chamada do dia e acompanhe as faltas. Atraso conta como presença; falta justificada não reduz a frequência."));
    subtitulo->setObjectName(QStringLiteral("pageSubtitle"));
    subtitulo->setWordWrap(true);
    raiz->addWidget(titulo);
    raiz->addWidget(subtitulo);

    auto *barra = new QHBoxLayout;
    m_comboTurma = new QComboBox;
    m_comboTurma->setMinimumWidth(260);
    barra->addWidget(new QLabel(QStringLiteral("Turma:")));
    barra->addWidget(m_comboTurma);
    barra->addStretch(1);
    raiz->addLayout(barra);

    m_abas = new QTabWidget;
    raiz->addWidget(m_abas, 1);

    // ===================== Aba: chamada do dia =====================
    auto *abaChamada = new QWidget;
    auto *lc = new QVBoxLayout(abaChamada);
    lc->setContentsMargins(0, 12, 0, 0);

    auto *linhaData = new QHBoxLayout;
    auto *btnAnterior = new QPushButton;
    auto *btnProximo = new QPushButton;
    btnAnterior->setToolTip(QStringLiteral("Dia anterior"));
    btnProximo->setToolTip(QStringLiteral("Próximo dia"));
    ThemeManager::iconeNoBotao(btnAnterior, QStringLiteral("seta-esquerda"), Tokens::Id::Ink, 16);
    ThemeManager::iconeNoBotao(btnProximo, QStringLiteral("seta-direita"), Tokens::Id::Ink, 16);
    auto *btnHoje = new QPushButton(QStringLiteral("Hoje"));
    m_data = new QDateEdit(QDate::currentDate());
    m_data->setCalendarPopup(true);
    m_data->setDisplayFormat(QStringLiteral("dddd, dd/MM/yyyy"));
    m_data->setLocale(ptBR());
    m_btnTodosPresentes = new QPushButton(QStringLiteral("Marcar todos como presentes"));
    m_btnTodosPresentes->setObjectName(QStringLiteral("primary"));
    ThemeManager::iconeNoBotao(m_btnTodosPresentes, QStringLiteral("check"), Tokens::Id::OnPrimary);
    linhaData->addWidget(btnAnterior);
    linhaData->addWidget(m_data);
    linhaData->addWidget(btnProximo);
    linhaData->addWidget(btnHoje);
    linhaData->addStretch(1);
    linhaData->addWidget(m_btnTodosPresentes);
    lc->addLayout(linhaData);

    m_aviso = new QLabel;
    ThemeManager::definirEstado(m_aviso, ThemeManager::Estado::Aviso);
    m_aviso->setWordWrap(true);
    lc->addWidget(m_aviso);

    m_tabelaChamada = new QTableWidget(0, 3);
    m_tabelaChamada->setHorizontalHeaderLabels({QStringLiteral("Aluno"), QStringLiteral("Situação"),
                                                QStringLiteral("Justificativa / observação")});
    m_tabelaChamada->setSelectionMode(QAbstractItemView::NoSelection);
    m_tabelaChamada->setAlternatingRowColors(true);
    m_tabelaChamada->setShowGrid(false);
    m_tabelaChamada->verticalHeader()->setVisible(false);
    m_tabelaChamada->verticalHeader()->setDefaultSectionSize(40);
    m_tabelaChamada->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tabelaChamada->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_tabelaChamada->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tabelaChamada->setColumnWidth(1, 190);
    lc->addWidget(m_tabelaChamada, 1);

    m_resumoDia = new QLabel;
    m_resumoDia->setObjectName(QStringLiteral("muted"));
    lc->addWidget(m_resumoDia);
    m_abas->addTab(abaChamada, QStringLiteral("Chamada do dia"));

    // ===================== Aba: resumo do mês =====================
    auto *abaMes = new QWidget;
    auto *lm = new QVBoxLayout(abaMes);
    lm->setContentsMargins(0, 12, 0, 0);

    auto *linhaMes = new QHBoxLayout;
    m_mes = new QComboBox;
    for (int i = 1; i <= 12; ++i) {
        QString nome = ptBR().monthName(i);
        nome[0] = nome[0].toUpper();
        m_mes->addItem(nome, i);
    }
    m_mes->setCurrentIndex(QDate::currentDate().month() - 1);
    m_ano = new QSpinBox;
    m_ano->setRange(2000, 2100);
    m_ano->setValue(QDate::currentDate().year());
    linhaMes->addWidget(m_mes);
    linhaMes->addWidget(m_ano);
    linhaMes->addStretch(1);
    lm->addLayout(linhaMes);

    m_tabelaMes = new QTableWidget;
    m_tabelaMes->setSelectionMode(QAbstractItemView::NoSelection);
    m_tabelaMes->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tabelaMes->verticalHeader()->setVisible(false);
    lm->addWidget(m_tabelaMes, 1);

    m_legendaMes = new QLabel;
    auto montarLegenda = [this] {
        m_legendaMes->setText(QStringLiteral(
            "• presente   A atraso   <span style='color:%2'>F falta</span>   "
            "<span style='color:%3'>J justificada</span>   ·   Freq. = frequência geral da turma no ano "
            "(vermelho: abaixo de %1%)")
                                  .arg(FrequenciaUtil::kFrequenciaMinima, 0, 'f', 0)
                                  .arg(ThemeManager::corHex(Tokens::Id::Danger),
                                       ThemeManager::corHex(Tokens::Id::Warning)));
    };
    montarLegenda();
    connect(&ThemeManager::notificador(), &ThemeNotifier::temaMudou, this, montarLegenda);
    m_legendaMes->setTextFormat(Qt::RichText);
    m_legendaMes->setObjectName(QStringLiteral("muted"));
    m_legendaMes->setWordWrap(true);
    lm->addWidget(m_legendaMes);
    m_abas->addTab(abaMes, QStringLiteral("Resumo do mês"));

    // ===================== Conexões =====================
    // Cores da chamada e do resumo (itens e combos) acompanham o tema.
    connect(&ThemeManager::notificador(), &ThemeNotifier::temaMudou, this, [this] {
        if (isVisible()) {
            recarregarChamada();
            recarregarResumo();
        }
    });
    connect(m_comboTurma, &QComboBox::currentIndexChanged, this, [this] {
        recarregarChamada();
        recarregarResumo();
    });
    connect(m_data, &QDateEdit::dateChanged, this, [this] { recarregarChamada(); });
    connect(btnAnterior, &QPushButton::clicked, this, [this] { mudarDia(-1); });
    connect(btnProximo, &QPushButton::clicked, this, [this] { mudarDia(+1); });
    connect(btnHoje, &QPushButton::clicked, this, [this] { m_data->setDate(QDate::currentDate()); });
    connect(m_btnTodosPresentes, &QPushButton::clicked, this, [this] {
        m_frequencia.marcarRestantes(turmaAtualId(), m_data->date(), QLatin1Char('P'));
        recarregarChamada();
        recarregarResumo();
    });
    connect(m_mes, &QComboBox::currentIndexChanged, this, [this] { recarregarResumo(); });
    connect(m_ano, &QSpinBox::valueChanged, this, [this] { recarregarResumo(); });
    connect(m_abas, &QTabWidget::currentChanged, this, [this] { recarregarResumo(); });
}

void FrequenciaPage::showEvent(QShowEvent *evento)
{
    QWidget::showEvent(evento);
    recarregarTurmas();
}

int FrequenciaPage::turmaAtualId() const
{
    return m_comboTurma->currentData().toInt();
}

void FrequenciaPage::mudarDia(int dias)
{
    m_data->setDate(m_data->date().addDays(dias));
}

void FrequenciaPage::recarregarTurmas()
{
    const int anterior = turmaAtualId();
    m_comboTurma->blockSignals(true);
    m_comboTurma->clear();
    for (const Turma &t : m_turmas.listar(false))
        m_comboTurma->addItem(t.disciplina.isEmpty() ? t.nome : QStringLiteral("%1 — %2").arg(t.nome, t.disciplina), t.id);
    const int idx = m_comboTurma->findData(anterior);
    m_comboTurma->setCurrentIndex(idx >= 0 ? idx : (m_comboTurma->count() > 0 ? 0 : -1));
    m_comboTurma->blockSignals(false);

    recarregarChamada();
    recarregarResumo();
}

// ============================================================================
// Chamada do dia
// ============================================================================

void FrequenciaPage::recarregarChamada()
{
    m_tabelaChamada->setRowCount(0);
    const int turmaId = turmaAtualId();
    const QDate data = m_data->date();

    // Aviso de fim de semana / feriado (não impede a chamada).
    QString aviso;
    if (data.dayOfWeek() >= 6)
        aviso = QStringLiteral("Atenção: este dia cai no fim de semana.");
    const QString motivo = m_eventos.motivoDeDiaSemAula(data);
    if (!motivo.isEmpty())
        aviso = QStringLiteral("Atenção: dia marcado como sem aula no calendário: %1.").arg(motivo);
    m_aviso->setText(aviso);
    m_aviso->setVisible(!aviso.isEmpty());

    m_btnTodosPresentes->setEnabled(turmaId != 0);
    if (turmaId == 0) {
        m_resumoDia->setText(QStringLiteral("Cadastre uma turma e seus alunos na seção Turmas."));
        return;
    }

    const auto registros = m_frequencia.doDia(turmaId, data);
    const QList<Aluno> alunos = m_alunos.listarPorTurma(turmaId, QString(), /*incluirInativos=*/false);
    m_tabelaChamada->setRowCount(alunos.size());

    for (int linha = 0; linha < alunos.size(); ++linha) {
        const Aluno &aluno = alunos.at(linha);
        const RegistroFrequencia registro = registros.value(aluno.id);
        const bool temRegistro = registros.contains(aluno.id);

        auto *itemNome = new QTableWidgetItem(aluno.nome);
        itemNome->setFlags(Qt::ItemIsEnabled);
        m_tabelaChamada->setItem(linha, 0, itemNome);

        auto *combo = new QComboBox;
        combo->addItem(QStringLiteral("— sem registro"), QString());
        combo->addItem(QStringLiteral("Presente"), QStringLiteral("P"));
        combo->addItem(QStringLiteral("Falta"), QStringLiteral("F"));
        combo->addItem(QStringLiteral("Falta justificada"), QStringLiteral("J"));
        combo->addItem(QStringLiteral("Atraso"), QStringLiteral("A"));
        combo->setCurrentIndex(temRegistro ? qMax(0, combo->findData(QString(registro.situacao))) : 0);
        m_tabelaChamada->setCellWidget(linha, 1, combo);

        auto *justificativa = new QLineEdit(temRegistro ? registro.justificativa : QString());
        justificativa->setPlaceholderText(QStringLiteral("Motivo (opcional)"));
        justificativa->setEnabled(combo->currentData().toString() == QLatin1String("F") ||
                                  combo->currentData().toString() == QLatin1String("J"));
        m_tabelaChamada->setCellWidget(linha, 2, justificativa);

        auto pintar = [combo] {
            const QColor cor = ThemeManager::corDaSituacao(combo->currentData().toString());
            combo->setStyleSheet(cor.isValid() ? QStringLiteral("QComboBox { color: %1; font-weight: 600; }").arg(cor.name())
                                               : QString());
        };
        pintar();

        // Grava na hora ao mudar a situação ou o texto da justificativa.
        const int alunoId = aluno.id;
        auto gravar = [this, alunoId, combo, justificativa, data, pintar] {
            const QString situacao = combo->currentData().toString();
            justificativa->setEnabled(situacao == QLatin1String("F") || situacao == QLatin1String("J"));
            pintar();
            if (situacao.isEmpty())
                m_frequencia.remover(alunoId, data);
            else
                m_frequencia.salvar(alunoId, data, situacao.at(0), justificativa->text().trimmed());
            atualizarResumoDoDia();
        };
        connect(combo, &QComboBox::currentIndexChanged, this, gravar);
        connect(justificativa, &QLineEdit::editingFinished, this, gravar);
    }
    atualizarResumoDoDia();
}

void FrequenciaPage::atualizarResumoDoDia()
{
    int p = 0, f = 0, j = 0, a = 0, semRegistro = 0;
    for (int i = 0; i < m_tabelaChamada->rowCount(); ++i) {
        auto *combo = qobject_cast<QComboBox *>(m_tabelaChamada->cellWidget(i, 1));
        if (!combo)
            continue;
        const QString s = combo->currentData().toString();
        if (s == QLatin1String("P")) ++p;
        else if (s == QLatin1String("F")) ++f;
        else if (s == QLatin1String("J")) ++j;
        else if (s == QLatin1String("A")) ++a;
        else ++semRegistro;
    }
    m_resumoDia->setText(QStringLiteral("%1 presente(s) · %2 atraso(s) · %3 falta(s) · %4 justificada(s) · %5 sem registro")
                             .arg(p).arg(a).arg(f).arg(j).arg(semRegistro));
}

// ============================================================================
// Resumo do mês
// ============================================================================

void FrequenciaPage::recarregarResumo()
{
    m_tabelaMes->clear();
    m_tabelaMes->setRowCount(0);
    m_tabelaMes->setColumnCount(0);

    const int turmaId = turmaAtualId();
    if (turmaId == 0)
        return;

    const int ano = m_ano->value();
    const int mes = m_mes->currentData().toInt();
    const int diasNoMes = QDate(ano, mes, 1).daysInMonth();
    const QList<Aluno> alunos = m_alunos.listarPorTurma(turmaId, QString(), false);

    // Registros do mês indexados por (aluno, dia).
    QHash<QPair<int, int>, QChar> codigos;
    for (const RegistroFrequencia &r : m_frequencia.doMes(turmaId, ano, mes))
        codigos.insert({r.alunoId, r.data.day()}, r.situacao);
    const auto geral = m_frequencia.resumoPorAluno(turmaId);

    const int colFaltas = diasNoMes + 1;
    const int colFreq = diasNoMes + 2;
    m_tabelaMes->setColumnCount(diasNoMes + 3);
    m_tabelaMes->setRowCount(alunos.size());

    QStringList cabecalhos = {QStringLiteral("Aluno")};
    for (int d = 1; d <= diasNoMes; ++d)
        cabecalhos << QString::number(d);
    cabecalhos << QStringLiteral("Faltas") << QStringLiteral("Freq.");
    m_tabelaMes->setHorizontalHeaderLabels(cabecalhos);
    m_tabelaMes->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_tabelaMes->setColumnWidth(0, 220);
    for (int c = 1; c <= diasNoMes; ++c)
        m_tabelaMes->setColumnWidth(c, 30);
    m_tabelaMes->setColumnWidth(colFaltas, 60);
    m_tabelaMes->setColumnWidth(colFreq, 70);

    for (int linha = 0; linha < alunos.size(); ++linha) {
        const Aluno &aluno = alunos.at(linha);

        auto *nome = new QTableWidgetItem(aluno.nome);
        const ResumoFrequencia resumo = geral.value(aluno.id);
        const auto pct = FrequenciaUtil::percentual(resumo.presencas, resumo.atrasos, resumo.faltas, resumo.justificadas);
        if (pct && *pct < FrequenciaUtil::kFrequenciaMinima)
            nome->setForeground(QBrush(ThemeManager::cor(Tokens::Id::Danger)));
        m_tabelaMes->setItem(linha, 0, nome);

        int faltasNoMes = 0;
        for (int d = 1; d <= diasNoMes; ++d) {
            const QChar c = codigos.value({aluno.id, d});
            QString texto;
            switch (c.toLatin1()) {
            case 'P': texto = QStringLiteral("•"); break;
            case 'F': texto = QStringLiteral("F"); ++faltasNoMes; break;
            case 'J': texto = QStringLiteral("J"); break;
            case 'A': texto = QStringLiteral("A"); break;
            default: break;
            }
            auto *item = new QTableWidgetItem(texto);
            item->setTextAlignment(Qt::AlignCenter);
            const QColor cor = ThemeManager::corDaSituacao(QString(c));
            if (cor.isValid() && c != QLatin1Char('P'))
                item->setForeground(QBrush(cor));
            // Fins de semana com fundo levemente diferente; o atraso (A) ganha o ocre suave.
            if (QDate(ano, mes, d).dayOfWeek() >= 6)
                item->setBackground(QBrush(ThemeManager::cor(Tokens::Id::Surface300)));
            const QColor fundo = ThemeManager::fundoDaSituacao(QString(c));
            if (fundo.isValid())
                item->setBackground(QBrush(fundo));
            m_tabelaMes->setItem(linha, d, item);
        }

        auto *itemFaltas = new QTableWidgetItem(QString::number(faltasNoMes));
        itemFaltas->setTextAlignment(Qt::AlignCenter);
        m_tabelaMes->setItem(linha, colFaltas, itemFaltas);

        auto *itemFreq = new QTableWidgetItem(pct ? QStringLiteral("%1%").arg(QString::number(*pct, 'f', 1).replace('.', ',')) : QStringLiteral("—"));
        itemFreq->setTextAlignment(Qt::AlignCenter);
        if (pct && *pct < FrequenciaUtil::kFrequenciaMinima)
            itemFreq->setForeground(QBrush(ThemeManager::cor(Tokens::Id::Danger)));
        m_tabelaMes->setItem(linha, colFreq, itemFreq);
    }
}
