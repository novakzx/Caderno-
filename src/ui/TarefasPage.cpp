#include "ui/TarefasPage.h"
#include "ui/EstadoVazio.h"
#include "ui/ThemeManager.h"

#include "database/Repositorios.h"
#include "database/TarefaRepository.h"
#include "database/TurmaRepository.h"
#include "ui/TarefaDialog.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
constexpr int kRoleId = Qt::UserRole;

QString nomeDaPrioridade(int p)
{
    return p >= 2 ? QStringLiteral("Alta") : (p == 0 ? QStringLiteral("Baixa") : QStringLiteral("Normal"));
}
}  // namespace

TarefasPage::TarefasPage(Repositorios &repos, QWidget *parent)
    : QWidget(parent), m_tarefas(repos.tarefas), m_turmas(repos.turmas)
{
    auto *raiz = new QVBoxLayout(this);
    raiz->setContentsMargins(32, 28, 32, 24);
    raiz->setSpacing(8);

    auto *titulo = new QLabel(QStringLiteral("Tarefas"));
    titulo->setObjectName(QStringLiteral("pageTitle"));
    auto *subtitulo = new QLabel(QStringLiteral("O que você precisa fazer: corrigir, preparar, entregar. Marque a caixa para concluir."));
    subtitulo->setObjectName(QStringLiteral("pageSubtitle"));
    raiz->addWidget(titulo);
    raiz->addWidget(subtitulo);
    raiz->addSpacing(8);

    auto *barra = new QHBoxLayout;
    m_filtroSituacao = new QComboBox;
    m_filtroSituacao->addItem(QStringLiteral("Pendentes"), static_cast<int>(TarefaRepository::Filtro::Pendentes));
    m_filtroSituacao->addItem(QStringLiteral("Concluídas"), static_cast<int>(TarefaRepository::Filtro::Concluidas));
    m_filtroSituacao->addItem(QStringLiteral("Todas"), static_cast<int>(TarefaRepository::Filtro::Todas));
    m_filtroTurma = new QComboBox;
    m_filtroTurma->setMinimumWidth(200);
    auto *btnNova = new QPushButton(QStringLiteral("+ Nova tarefa"));
    btnNova->setObjectName(QStringLiteral("primary"));
    m_btnEditar = new QPushButton(QStringLiteral("Editar"));
    m_btnExcluir = new QPushButton(QStringLiteral("Excluir"));
    m_btnExcluir->setObjectName(QStringLiteral("danger"));
    barra->addWidget(m_filtroSituacao);
    barra->addWidget(m_filtroTurma);
    barra->addStretch(1);
    barra->addWidget(btnNova);
    barra->addWidget(m_btnEditar);
    barra->addWidget(m_btnExcluir);
    raiz->addLayout(barra);

    m_tabela = new QTableWidget(0, 5);
    m_tabela->setHorizontalHeaderLabels({QString(), QStringLiteral("Tarefa"), QStringLiteral("Turma"),
                                         QStringLiteral("Prazo"), QStringLiteral("Prioridade")});
    m_tabela->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tabela->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tabela->setAlternatingRowColors(true);
    m_tabela->setShowGrid(false);
    m_tabela->verticalHeader()->setVisible(false);
    m_tabela->verticalHeader()->setDefaultSectionSize(36);
    m_tabela->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tabela->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_tabela->setColumnWidth(0, 44);
    m_tabela->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tabela->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tabela->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tabela->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    EstadoVazio::sobre(m_tabela, QStringLiteral("tarefas"), QStringLiteral("Nenhuma tarefa por aqui"),
                       QStringLiteral("Clique em \"+ Nova tarefa\" para anotar o que precisa fazer."));
    raiz->addWidget(m_tabela, 1);

    m_resumo = new QLabel;
    m_resumo->setObjectName(QStringLiteral("muted"));
    raiz->addWidget(m_resumo);

    // As cores dos itens (atraso, prioridade, concluída) acompanham o tema.
    connect(&ThemeManager::notificador(), &ThemeNotifier::temaMudou, this, [this] {
        if (isVisible())
            recarregar(tarefaSelecionadaId());
    });
    connect(btnNova, &QPushButton::clicked, this, &TarefasPage::nova);
    connect(m_btnEditar, &QPushButton::clicked, this, &TarefasPage::editar);
    connect(m_btnExcluir, &QPushButton::clicked, this, &TarefasPage::excluir);
    connect(m_filtroSituacao, &QComboBox::currentIndexChanged, this, [this] { if (!m_carregando) recarregar(); });
    connect(m_filtroTurma, &QComboBox::currentIndexChanged, this, [this] { if (!m_carregando) recarregar(); });
    connect(m_tabela, &QTableWidget::itemSelectionChanged, this, &TarefasPage::atualizarBotoes);
    connect(m_tabela, &QTableWidget::cellDoubleClicked, this, [this](int, int coluna) { if (coluna != 0) editar(); });
    connect(m_tabela, &QTableWidget::itemChanged, this, &TarefasPage::aoAlterarItem);

    atualizarBotoes();
}

void TarefasPage::showEvent(QShowEvent *evento)
{
    QWidget::showEvent(evento);
    recarregarTurmas();
}

void TarefasPage::recarregarTurmas()
{
    m_carregando = true;
    const int anterior = m_filtroTurma->currentData().toInt();
    m_filtroTurma->clear();
    m_filtroTurma->addItem(QStringLiteral("Todas as turmas"), 0);
    for (const Turma &t : m_turmas.listar(false))
        m_filtroTurma->addItem(t.nome, t.id);
    const int idx = m_filtroTurma->findData(anterior);
    m_filtroTurma->setCurrentIndex(idx >= 0 ? idx : 0);
    m_carregando = false;
    recarregar(tarefaSelecionadaId());
}

int TarefasPage::tarefaSelecionadaId() const
{
    const auto linhas = m_tabela->selectionModel()->selectedRows();
    if (linhas.isEmpty())
        return 0;
    return m_tabela->item(linhas.first().row(), 1)->data(kRoleId).toInt();
}

void TarefasPage::recarregar(int selecionarId)
{
    const auto filtro = static_cast<TarefaRepository::Filtro>(m_filtroSituacao->currentData().toInt());
    const QList<Tarefa> tarefas = m_tarefas.listar(filtro, m_filtroTurma->currentData().toInt());
    const QDate hoje = QDate::currentDate();

    m_carregando = true;  // evita tratar o preenchimento como se o usuário tivesse marcado caixas
    m_tabela->setRowCount(0);
    m_tabela->setRowCount(tarefas.size());
    int linhaSelecionada = -1;
    int atrasadas = 0;

    for (int i = 0; i < tarefas.size(); ++i) {
        const Tarefa &t = tarefas.at(i);

        auto *caixa = new QTableWidgetItem;
        caixa->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        caixa->setCheckState(t.concluida ? Qt::Checked : Qt::Unchecked);
        m_tabela->setItem(i, 0, caixa);

        auto *nome = new QTableWidgetItem(t.titulo);
        nome->setData(kRoleId, t.id);
        nome->setToolTip(t.descricao);
        if (t.concluida) {
            QFont f = nome->font();
            f.setStrikeOut(true);
            nome->setFont(f);
            nome->setForeground(QBrush(ThemeManager::cor(Tokens::Id::InkMuted)));
        }
        m_tabela->setItem(i, 1, nome);
        m_tabela->setItem(i, 2, new QTableWidgetItem(t.turmaNome));

        auto *prazo = new QTableWidgetItem(t.dataEntrega.isValid() ? t.dataEntrega.toString(QStringLiteral("dd/MM/yyyy")) : QStringLiteral("—"));
        if (!t.concluida && t.dataEntrega.isValid() && t.dataEntrega < hoje) {
            prazo->setForeground(QBrush(ThemeManager::cor(Tokens::Id::Danger)));
            prazo->setText(prazo->text() + QStringLiteral(" (atrasada)"));
            ++atrasadas;
        }
        m_tabela->setItem(i, 3, prazo);

        auto *prioridade = new QTableWidgetItem(nomeDaPrioridade(t.prioridade));
        if (t.prioridade >= 2)
            prioridade->setForeground(QBrush(ThemeManager::cor(Tokens::Id::Danger)));
        m_tabela->setItem(i, 4, prioridade);

        if (t.id == selecionarId)
            linhaSelecionada = i;
    }
    m_carregando = false;

    if (linhaSelecionada >= 0)
        m_tabela->selectRow(linhaSelecionada);
    m_resumo->setText(QStringLiteral("%1 tarefa(s)%2")
                          .arg(tarefas.size())
                          .arg(atrasadas > 0 ? QStringLiteral(" · %1 atrasada(s)").arg(atrasadas) : QString()));
    atualizarBotoes();
}

void TarefasPage::atualizarBotoes()
{
    const bool ha = tarefaSelecionadaId() != 0;
    m_btnEditar->setEnabled(ha);
    m_btnExcluir->setEnabled(ha);
}

void TarefasPage::aoAlterarItem(QTableWidgetItem *item)
{
    if (m_carregando || item->column() != 0)
        return;
    const int id = m_tabela->item(item->row(), 1)->data(kRoleId).toInt();
    if (!m_tarefas.marcarConcluida(id, item->checkState() == Qt::Checked))
        QMessageBox::critical(this, QStringLiteral("Erro"), m_tarefas.ultimoErro());
    recarregar(id);
}

void TarefasPage::nova()
{
    TarefaDialog dlg(m_turmas.listar(false), Tarefa(), false, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const int id = m_tarefas.inserir(dlg.tarefa());
    if (id == 0) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_tarefas.ultimoErro());
        return;
    }
    // Garante que a nova tarefa aparece (o filtro "Concluídas" a esconderia).
    if (dlg.tarefa().concluida != (m_filtroSituacao->currentData().toInt() == static_cast<int>(TarefaRepository::Filtro::Concluidas)))
        m_filtroSituacao->setCurrentIndex(m_filtroSituacao->findData(static_cast<int>(TarefaRepository::Filtro::Todas)));
    recarregar(id);
}

void TarefasPage::editar()
{
    const int id = tarefaSelecionadaId();
    const auto existente = m_tarefas.buscar(id);
    if (!existente)
        return;
    TarefaDialog dlg(m_turmas.listar(false), *existente, true, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    if (!m_tarefas.atualizar(dlg.tarefa())) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_tarefas.ultimoErro());
        return;
    }
    recarregar(id);
}

void TarefasPage::excluir()
{
    const int id = tarefaSelecionadaId();
    const auto tarefa = m_tarefas.buscar(id);
    if (!tarefa)
        return;
    const auto resp = QMessageBox::question(this, QStringLiteral("Excluir tarefa"),
                                            QStringLiteral("Excluir a tarefa \"%1\"?").arg(tarefa->titulo),
                                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (resp != QMessageBox::Yes)
        return;
    if (!m_tarefas.remover(id)) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_tarefas.ultimoErro());
        return;
    }
    recarregar();
}

void TarefasPage::selecionarTarefa(int tarefaId)
{
    m_carregando = true;
    m_filtroSituacao->setCurrentIndex(m_filtroSituacao->findData(static_cast<int>(TarefaRepository::Filtro::Todas)));
    m_filtroTurma->setCurrentIndex(0);
    m_carregando = false;
    recarregar(tarefaId);
}
