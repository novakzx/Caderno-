#include "ui/HorarioPage.h"
#include "ui/ThemeManager.h"

#include "database/HorarioRepository.h"
#include "database/TurmaRepository.h"
#include "ui/HorarioDialog.h"
#include "ui/HorarioGridWidget.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSet>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace {
constexpr int kDuracaoPadraoMin = 50;  // duração de uma aula nova (aula de 50 minutos)
}

HorarioPage::HorarioPage(HorarioRepository &horarios, TurmaRepository &turmas, QWidget *parent)
    : QWidget(parent), m_horarios(horarios), m_turmas(turmas)
{
    auto *raiz = new QVBoxLayout(this);
    raiz->setContentsMargins(32, 28, 32, 20);
    raiz->setSpacing(8);

    auto *titulo = new QLabel(QStringLiteral("Horário"));
    titulo->setObjectName(QStringLiteral("pageTitle"));
    auto *subtitulo = new QLabel(QStringLiteral(
        "Arraste uma aula para mudar de dia ou horário; arraste a borda de baixo para mudar a duração. "
        "Duplo clique no vazio cria uma aula; duplo clique numa aula edita."));
    subtitulo->setObjectName(QStringLiteral("pageSubtitle"));
    subtitulo->setWordWrap(true);
    raiz->addWidget(titulo);
    raiz->addWidget(subtitulo);
    raiz->addSpacing(6);

    // Barra de ferramentas
    auto *barra = new QHBoxLayout;
    m_btnNovaAula = new QPushButton(QStringLiteral("+ Aula"));
    m_btnNovaAula->setObjectName(QStringLiteral("primary"));
    m_fimDeSemana = new QCheckBox(QStringLiteral("Mostrar sábado e domingo"));
    m_fimDeSemana->setChecked(QSettings().value(QStringLiteral("horarioFimDeSemana"), false).toBool());
    barra->addWidget(m_btnNovaAula);
    barra->addWidget(m_fimDeSemana);
    barra->addStretch(1);
    raiz->addLayout(barra);

    // Legenda de cores (turmas que aparecem na grade)
    m_legenda = new QHBoxLayout;
    m_legenda->setSpacing(14);
    raiz->addLayout(m_legenda);

    m_dica = new QLabel;
    m_dica->setObjectName(QStringLiteral("muted"));
    m_dica->setWordWrap(true);
    raiz->addWidget(m_dica);

    // Grade dentro de uma área de rolagem (horários longos cabem rolando)
    m_grade = new HorarioGridWidget;
    auto *rolagem = new QScrollArea;
    rolagem->setWidgetResizable(true);
    rolagem->setFrameShape(QFrame::NoFrame);
    rolagem->setWidget(m_grade);
    raiz->addWidget(rolagem, 1);

    m_mensagem = new QLabel;
    m_mensagem->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    raiz->addWidget(m_mensagem);

    m_timerMensagem = new QTimer(this);
    m_timerMensagem->setSingleShot(true);
    connect(m_timerMensagem, &QTimer::timeout, m_mensagem, &QLabel::clear);
    // A legenda das turmas acompanha o tema (a grade se repinta sozinha).
    connect(&ThemeManager::notificador(), &ThemeNotifier::temaMudou, this, [this] {
        if (isVisible())
            recarregar();
    });

    // A linha do "agora" anda: repinta a cada minuto.
    m_timerRelogio = new QTimer(this);
    connect(m_timerRelogio, &QTimer::timeout, m_grade, qOverload<>(&QWidget::update));
    m_timerRelogio->start(60 * 1000);

    // Conexões
    connect(m_btnNovaAula, &QPushButton::clicked, this, [this] { novaAula(1, QTime(7, 30)); });
    connect(m_fimDeSemana, &QCheckBox::toggled, this, [this](bool marcado) {
        QSettings().setValue(QStringLiteral("horarioFimDeSemana"), marcado);
        recarregar();
    });
    connect(m_grade, &HorarioGridWidget::novaAulaSolicitada, this, &HorarioPage::novaAula);
    connect(m_grade, &HorarioGridWidget::editarSolicitado, this, &HorarioPage::editarAula);
    connect(m_grade, &HorarioGridWidget::excluirSolicitado, this, &HorarioPage::excluirAula);
    connect(m_grade, &HorarioGridWidget::aulaAlterada, this, &HorarioPage::alterarAula);
    connect(m_grade, &HorarioGridWidget::movimentoRecusado, this, [this] {
        mostrarMensagem(QStringLiteral("Esse horário já está ocupado por outra aula."), true);
    });
}

void HorarioPage::showEvent(QShowEvent *evento)
{
    QWidget::showEvent(evento);
    recarregar();  // as turmas (nome, cor, arquivadas) podem ter mudado em outra tela
}

// ============================================================================

void HorarioPage::recarregar()
{
    const QList<Horario> aulas = m_horarios.listar();

    // Janela de horas: no mínimo 07:00-18:00, ampliada para caber todas as aulas.
    int horaIni = 7, horaFim = 18;
    bool temFimDeSemana = false;
    for (const Horario &a : aulas) {
        horaIni = std::min(horaIni, a.inicio.hour());
        horaFim = std::max(horaFim, a.fim.minute() > 0 ? a.fim.hour() + 1 : a.fim.hour());
        if (a.diaSemana >= 6)
            temFimDeSemana = true;
    }

    // Com aulas no fim de semana, a grade mostra esses dias mesmo com a caixa desmarcada.
    int dias = 5;
    if (m_fimDeSemana->isChecked() || temFimDeSemana)
        dias = 7;
    m_grade->setDados(aulas, dias, horaIni, horaFim);

    atualizarLegenda(aulas);

    if (m_turmas.listar(false).isEmpty())
        m_dica->setText(QStringLiteral("Nenhuma turma cadastrada. Crie turmas na seção Turmas antes de montar o horário."));
    else if (aulas.isEmpty())
        m_dica->setText(QStringLiteral("Nenhuma aula no horário ainda. Clique em \"+ Aula\" ou dê um duplo clique na grade."));
    else
        m_dica->clear();
    m_dica->setVisible(!m_dica->text().isEmpty());
    m_btnNovaAula->setEnabled(!m_turmas.listar(false).isEmpty());
}

void HorarioPage::atualizarLegenda(const QList<Horario> &aulas)
{
    // Esvazia a legenda anterior
    while (QLayoutItem *item = m_legenda->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    QSet<int> vistas;
    for (const Horario &a : aulas) {
        if (vistas.contains(a.turmaId))
            continue;
        vistas.insert(a.turmaId);

        auto *chip = new QLabel(QStringLiteral("<span style='color:%1'>■</span> %2").arg(ThemeManager::corDaTurmaHex(a.turmaCor), a.turmaNome.toHtmlEscaped()));
        chip->setTextFormat(Qt::RichText);
        m_legenda->addWidget(chip);
    }
    m_legenda->addStretch(1);
}

void HorarioPage::mostrarMensagem(const QString &texto, bool erro)
{
    ThemeManager::definirEstado(m_mensagem, erro ? ThemeManager::Estado::Erro : ThemeManager::Estado::Sucesso);
    m_mensagem->setText(texto);
    m_timerMensagem->start(5000);
}

QString HorarioPage::validarConflito(const Horario &h)
{
    if (m_horarios.temConflito(h.diaSemana, h.inicio, h.fim, h.id))
        return QStringLiteral("Já existe outra aula nesse dia que se sobrepõe a esse horário (%1–%2).")
            .arg(h.inicio.toString(QStringLiteral("HH:mm")), h.fim.toString(QStringLiteral("HH:mm")));
    return QString();
}

// ============================================================================
// Ações
// ============================================================================

void HorarioPage::novaAula(int diaSemana, const QTime &inicio)
{
    const QList<Turma> turmas = m_turmas.listar(false);
    if (turmas.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Sem turmas"),
                                 QStringLiteral("Cadastre uma turma na seção Turmas primeiro."));
        return;
    }

    Horario base;
    base.turmaId = turmas.first().id;
    base.diaSemana = diaSemana;
    base.inicio = inicio;
    base.fim = inicio.addSecs(kDuracaoPadraoMin * 60);

    HorarioDialog dlg(turmas, base, false, [this](const Horario &h) { return validarConflito(h); }, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    if (m_horarios.inserir(dlg.horario()) == 0) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_horarios.ultimoErro());
        return;
    }
    recarregar();
}

void HorarioPage::editarAula(int id)
{
    const auto existente = m_horarios.buscar(id);
    if (!existente)
        return;

    HorarioDialog dlg(m_turmas.listar(false), *existente, true,
                      [this](const Horario &h) { return validarConflito(h); }, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    if (!m_horarios.atualizar(dlg.horario())) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_horarios.ultimoErro());
        return;
    }
    recarregar();
}

void HorarioPage::excluirAula(int id)
{
    const auto aula = m_horarios.buscar(id);
    if (!aula)
        return;

    const auto resp = QMessageBox::question(
        this, QStringLiteral("Excluir aula"),
        QStringLiteral("Remover \"%1\" (%2–%3) do horário?\n\nIsso não apaga a turma nem os planos de aula.")
            .arg(aula->turmaNome, aula->inicio.toString(QStringLiteral("HH:mm")),
                 aula->fim.toString(QStringLiteral("HH:mm"))),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (resp != QMessageBox::Yes)
        return;

    if (!m_horarios.remover(id)) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_horarios.ultimoErro());
        return;
    }
    recarregar();
}

void HorarioPage::alterarAula(int id, int diaSemana, const QTime &inicio, const QTime &fim)
{
    auto aula = m_horarios.buscar(id);
    if (!aula)
        return;

    aula->diaSemana = diaSemana;
    aula->inicio = inicio;
    aula->fim = fim;

    // A grade já evita sobreposição, mas o banco é quem decide (outro bloco pode
    // ter sido alterado desde que a grade foi desenhada).
    const QString erro = validarConflito(*aula);
    if (!erro.isEmpty()) {
        mostrarMensagem(erro, true);
        recarregar();
        return;
    }
    if (!m_horarios.atualizar(*aula)) {
        mostrarMensagem(QStringLiteral("Não foi possível salvar: %1").arg(m_horarios.ultimoErro()), true);
        recarregar();
        return;
    }
    recarregar();
    mostrarMensagem(QStringLiteral("Aula movida para %1, %2–%3.")
                        .arg(QLocale(QLocale::Portuguese, QLocale::Brazil).dayName(diaSemana),
                             inicio.toString(QStringLiteral("HH:mm")), fim.toString(QStringLiteral("HH:mm"))),
                    false);
}
