#include "ui/NotasPage.h"
#include "ui/ThemeManager.h"

#include "database/AlunoRepository.h"
#include "database/AvaliacaoRepository.h"
#include "database/NotaRepository.h"
#include "database/TurmaRepository.h"
#include "services/ImportadorNotas.h"
#include "services/XlsxService.h"
#include "ui/AvaliacaoDialog.h"
#include "ui/ImportarNotasDialog.h"
#include "ui/NotasTableModel.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QShortcut>
#include <QStandardPaths>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <climits>

NotasPage::NotasPage(TurmaRepository &turmas, AlunoRepository &alunos,
                     AvaliacaoRepository &avaliacoes, NotaRepository &notas, QWidget *parent)
    : QWidget(parent), m_turmas(turmas), m_alunos(alunos), m_avaliacoes(avaliacoes), m_notas(notas)
{
    m_modelo = new NotasTableModel(avaliacoes, alunos, notas, this);

    auto *raiz = new QVBoxLayout(this);
    raiz->setContentsMargins(32, 28, 32, 20);
    raiz->setSpacing(8);

    auto *titulo = new QLabel(QStringLiteral("Notas"));
    titulo->setObjectName(QStringLiteral("pageTitle"));
    auto *subtitulo = new QLabel(QStringLiteral(
        "Clique numa célula e digite a nota (vírgula ou ponto). A média ponderada é calculada automaticamente."));
    subtitulo->setObjectName(QStringLiteral("pageSubtitle"));
    raiz->addWidget(titulo);
    raiz->addWidget(subtitulo);
    raiz->addSpacing(8);

    // ---------------- Barra 1: turma, período, nota de corte, Excel ----------------
    auto *barra1 = new QHBoxLayout;
    barra1->setSpacing(10);

    m_comboTurma = new QComboBox;
    m_comboTurma->setMinimumWidth(240);
    m_comboPeriodo = new QComboBox;
    m_comboPeriodo->addItem(QStringLiteral("Todos os períodos"), 0);
    for (int p = 1; p <= 4; ++p)
        m_comboPeriodo->addItem(QStringLiteral("%1º período").arg(p), p);

    m_notaCorte = new QDoubleSpinBox;
    m_notaCorte->setRange(0.0, 100.0);
    m_notaCorte->setDecimals(1);
    m_notaCorte->setSingleStep(0.5);
    m_notaCorte->setValue(QSettings().value(QStringLiteral("notaCorte"), 6.0).toDouble());
    m_notaCorte->setToolTip(QStringLiteral("Médias abaixo deste valor aparecem em vermelho"));
    m_modelo->setNotaCorte(m_notaCorte->value());

    m_btnImportar = new QPushButton(QStringLiteral("⬆ Importar Excel"));
    m_btnExportar = new QPushButton(QStringLiteral("⬇ Exportar Excel"));

    barra1->addWidget(new QLabel(QStringLiteral("Turma:")));
    barra1->addWidget(m_comboTurma);
    barra1->addWidget(m_comboPeriodo);
    barra1->addWidget(new QLabel(QStringLiteral("Nota de corte:")));
    barra1->addWidget(m_notaCorte);
    barra1->addStretch(1);
    barra1->addWidget(m_btnImportar);
    barra1->addWidget(m_btnExportar);
    raiz->addLayout(barra1);

    // ---------------- Barra 2: avaliações ----------------
    auto *barra2 = new QHBoxLayout;
    barra2->setSpacing(8);
    m_btnNovaAvaliacao = new QPushButton(QStringLiteral("+ Avaliação"));
    m_btnNovaAvaliacao->setObjectName(QStringLiteral("primary"));
    m_btnEditar = new QPushButton(QStringLiteral("Editar coluna"));
    m_btnExcluir = new QPushButton(QStringLiteral("Excluir coluna"));
    m_btnExcluir->setObjectName(QStringLiteral("danger"));
    m_btnEsquerda = new QPushButton(QStringLiteral("◀"));
    m_btnDireita = new QPushButton(QStringLiteral("▶"));
    m_btnEsquerda->setToolTip(QStringLiteral("Mover a coluna selecionada para a esquerda"));
    m_btnDireita->setToolTip(QStringLiteral("Mover a coluna selecionada para a direita"));
    barra2->addWidget(m_btnNovaAvaliacao);
    barra2->addWidget(m_btnEditar);
    barra2->addWidget(m_btnExcluir);
    barra2->addWidget(m_btnEsquerda);
    barra2->addWidget(m_btnDireita);
    barra2->addStretch(1);
    raiz->addLayout(barra2);

    // ---------------- Dica (estado vazio) ----------------
    m_dica = new QLabel;
    m_dica->setObjectName(QStringLiteral("muted"));
    m_dica->setWordWrap(true);
    raiz->addWidget(m_dica);

    // ---------------- Planilha ----------------
    m_tabela = new QTableView;
    m_tabela->setModel(m_modelo);
    m_tabela->setAlternatingRowColors(true);
    m_tabela->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_tabela->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // Digitar já começa a editar; F2 ou duplo clique também.
    m_tabela->setEditTriggers(QAbstractItemView::AnyKeyPressed | QAbstractItemView::DoubleClicked |
                              QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    m_tabela->verticalHeader()->setDefaultSectionSize(34);
    m_tabela->verticalHeader()->setVisible(false);
    m_tabela->horizontalHeader()->setHighlightSections(false);
    m_tabela->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    raiz->addWidget(m_tabela, 1);

    // ---------------- Rodapé: resumo e mensagens ----------------
    auto *rodape = new QHBoxLayout;
    m_resumo = new QLabel;
    m_resumo->setObjectName(QStringLiteral("muted"));
    m_mensagem = new QLabel;
    m_mensagem->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rodape->addWidget(m_resumo, 1);
    rodape->addWidget(m_mensagem, 1);
    raiz->addLayout(rodape);

    m_timerMensagem = new QTimer(this);
    m_timerMensagem->setSingleShot(true);
    connect(m_timerMensagem, &QTimer::timeout, m_mensagem, &QLabel::clear);

    // ---------------- Atalhos de teclado da planilha ----------------
    // WidgetShortcut: valem só quando a planilha tem o foco (dentro do editor
    // de uma célula, Ctrl+C/V continuam copiando/colando o texto digitado).
    auto atalho = [this](const QKeySequence &seq, auto funcao) {
        auto *sc = new QShortcut(seq, m_tabela);
        sc->setContext(Qt::WidgetShortcut);
        connect(sc, &QShortcut::activated, this, funcao);
    };
    atalho(QKeySequence::Copy, [this] { copiarSelecao(); });
    atalho(QKeySequence::Paste, [this] { colarSelecao(); });
    atalho(QKeySequence(Qt::Key_Delete), [this] { limparSelecao(); });

    // ---------------- Conexões ----------------
    connect(m_comboTurma, &QComboBox::currentIndexChanged, this, [this] { recarregarModelo(); });
    connect(m_comboPeriodo, &QComboBox::currentIndexChanged, this, [this] { recarregarModelo(); });
    connect(m_notaCorte, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        QSettings().setValue(QStringLiteral("notaCorte"), v);
        m_modelo->setNotaCorte(v);
        atualizarEstado();
    });

    connect(m_btnNovaAvaliacao, &QPushButton::clicked, this, &NotasPage::novaAvaliacao);
    connect(m_btnEditar, &QPushButton::clicked, this, &NotasPage::editarAvaliacao);
    connect(m_btnExcluir, &QPushButton::clicked, this, &NotasPage::excluirAvaliacao);
    connect(m_btnEsquerda, &QPushButton::clicked, this, [this] { moverAvaliacao(-1); });
    connect(m_btnDireita, &QPushButton::clicked, this, [this] { moverAvaliacao(+1); });
    connect(m_btnImportar, &QPushButton::clicked, this, &NotasPage::importarExcel);
    connect(m_btnExportar, &QPushButton::clicked, this, &NotasPage::exportarExcel);

    connect(m_modelo, &NotasTableModel::mensagemDeErro, this,
            [this](const QString &msg) { mostrarMensagem(msg, true); });
    connect(m_modelo, &QAbstractItemModel::dataChanged, this, [this] { atualizarEstado(); });
    connect(m_modelo, &QAbstractItemModel::modelReset, this, [this] { atualizarEstado(); });
    connect(m_tabela->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this] { atualizarEstado(); });

    // O primeiro carregamento acontece no showEvent().
}

void NotasPage::showEvent(QShowEvent *evento)
{
    QWidget::showEvent(evento);
    recarregarTurmas();
}

// ============================================================================
// Carregamento
// ============================================================================

int NotasPage::turmaAtualId() const { return m_comboTurma->currentData().toInt(); }
int NotasPage::periodoAtual() const { return m_comboPeriodo->currentData().toInt(); }

int NotasPage::avaliacaoAtualId() const
{
    const auto *av = m_modelo->avaliacaoDaColuna(m_tabela->currentIndex().column());
    return av ? av->id : 0;
}

void NotasPage::recarregarTurmas()
{
    const int anterior = turmaAtualId();

    m_comboTurma->blockSignals(true);
    m_comboTurma->clear();
    for (const Turma &t : m_turmas.listar(false)) {
        QString texto = t.nome;
        if (!t.disciplina.isEmpty())
            texto += QStringLiteral(" — ") + t.disciplina;
        m_comboTurma->addItem(texto, t.id);
    }
    const int idx = m_comboTurma->findData(anterior);
    m_comboTurma->setCurrentIndex(idx >= 0 ? idx : (m_comboTurma->count() > 0 ? 0 : -1));
    m_comboTurma->blockSignals(false);

    recarregarModelo();
}

void NotasPage::recarregarModelo(int avaliacaoParaSelecionar)
{
    const int linhaAtual = qMax(0, m_tabela->currentIndex().row());
    const int colunaAtual = m_tabela->currentIndex().column();

    m_modelo->carregar(turmaAtualId(), periodoAtual());
    ajustarColunas();

    // Tenta manter o cursor onde estava (ou na coluna da avaliação pedida).
    if (m_modelo->rowCount() > 0 && m_modelo->columnCount() > 1) {
        int coluna = qBound(1, colunaAtual, m_modelo->columnCount() - 1);
        if (avaliacaoParaSelecionar != 0) {
            const int c = m_modelo->colunaDaAvaliacao(avaliacaoParaSelecionar);
            if (c > 0)
                coluna = c;
        }
        m_tabela->setCurrentIndex(m_modelo->index(qMin(linhaAtual, m_modelo->rowCount() - 1), coluna));
    }
    atualizarEstado();
}

void NotasPage::ajustarColunas()
{
    auto *h = m_tabela->horizontalHeader();
    const int n = m_modelo->columnCount();
    if (n == 0)
        return;
    h->setSectionResizeMode(0, QHeaderView::Interactive);
    m_tabela->setColumnWidth(0, 240);
    for (int c = 1; c < n; ++c) {
        h->setSectionResizeMode(c, QHeaderView::Interactive);
        m_tabela->setColumnWidth(c, c == m_modelo->colunaMedia() ? 90 : 130);
    }
    h->setMinimumHeight(52);  // cabeçalho com duas linhas (nome + peso/máx)
}

void NotasPage::atualizarEstado()
{
    const bool temTurma = turmaAtualId() != 0;
    const bool temAvaliacao = avaliacaoAtualId() != 0;

    m_btnNovaAvaliacao->setEnabled(temTurma);
    m_btnEditar->setEnabled(temAvaliacao);
    m_btnExcluir->setEnabled(temAvaliacao);
    m_btnEsquerda->setEnabled(temAvaliacao);
    m_btnDireita->setEnabled(temAvaliacao);
    m_btnExportar->setEnabled(temTurma);
    m_btnImportar->setEnabled(temTurma);
    m_comboPeriodo->setEnabled(temTurma);

    // Dica contextual quando a planilha está vazia.
    QString dica;
    if (!temTurma)
        dica = QStringLiteral("Nenhuma turma cadastrada. Crie uma turma e seus alunos na seção Turmas.");
    else if (m_modelo->totalAlunos() == 0)
        dica = QStringLiteral("Esta turma ainda não tem alunos ativos. Cadastre-os na seção Turmas "
                              "(ou importe uma planilha depois de cadastrá-los).");
    else if (m_modelo->totalAvaliacoes() == 0)
        dica = QStringLiteral("Clique em \"+ Avaliação\" para criar a primeira coluna (prova, trabalho...), "
                              "ou use \"Importar Excel\".");
    m_dica->setText(dica);
    m_dica->setVisible(!dica.isEmpty());

    // Resumo da turma
    const ResumoTurma r = m_modelo->resumo();
    if (r.mediaGeral)
        m_resumo->setText(QStringLiteral("Média da turma: %1 · %2 aluno(s) com média · %3 abaixo de %4")
                              .arg(QString::number(*r.mediaGeral, 'f', 2).replace('.', ','))
                              .arg(r.alunosComMedia)
                              .arg(r.abaixoDaCorte)
                              .arg(QString::number(m_notaCorte->value(), 'f', 1).replace('.', ',')));
    else
        m_resumo->setText(temTurma ? QStringLiteral("Ainda não há notas lançadas.") : QString());
}

void NotasPage::mostrarMensagem(const QString &texto, bool erro)
{
    ThemeManager::definirEstado(m_mensagem, erro ? ThemeManager::Estado::Erro : ThemeManager::Estado::Sucesso);
    m_mensagem->setText(texto);
    m_timerMensagem->start(6000);
}

// ============================================================================
// Avaliações (colunas)
// ============================================================================

void NotasPage::novaAvaliacao()
{
    if (turmaAtualId() == 0)
        return;
    AvaliacaoDialog dlg(turmaAtualId(), periodoAtual() == 0 ? 1 : periodoAtual(), nullptr, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const int id = m_avaliacoes.inserir(dlg.avaliacao());
    if (id == 0) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_avaliacoes.ultimoErro());
        return;
    }

    // Se a nova avaliação é de outro período que o filtro atual esconde, mostra todos.
    if (periodoAtual() != 0 && dlg.avaliacao().periodo != periodoAtual())
        m_comboPeriodo->setCurrentIndex(m_comboPeriodo->findData(dlg.avaliacao().periodo));
    recarregarModelo(id);
}

void NotasPage::editarAvaliacao()
{
    const auto existente = m_avaliacoes.buscar(avaliacaoAtualId());
    if (!existente)
        return;

    AvaliacaoDialog dlg(existente->turmaId, existente->periodo, &*existente, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const Avaliacao nova = dlg.avaliacao();

    // Reduzir a nota máxima pode deixar notas já lançadas fora do intervalo.
    if (nova.notaMaxima < existente->notaMaxima) {
        const int acima = m_notas.contarAcima(existente->id, nova.notaMaxima);
        if (acima > 0) {
            const auto resp = QMessageBox::question(
                this, QStringLiteral("Notas acima do novo máximo"),
                QStringLiteral("%1 nota(s) desta avaliação são maiores que %2 e continuarão gravadas "
                               "(a média vai normalizá-las como acima do máximo).\n\nDeseja continuar?")
                    .arg(acima)
                    .arg(nova.notaMaxima),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (resp != QMessageBox::Yes)
                return;
        }
    }

    if (!m_avaliacoes.atualizar(nova)) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_avaliacoes.ultimoErro());
        return;
    }
    recarregarModelo(nova.id);
}

void NotasPage::excluirAvaliacao()
{
    const auto av = m_avaliacoes.buscar(avaliacaoAtualId());
    if (!av)
        return;

    const auto resp = QMessageBox::question(
        this, QStringLiteral("Excluir avaliação"),
        QStringLiteral("Excluir a avaliação \"%1\"?\n\nTodas as notas lançadas nela serão apagadas. "
                       "Esta ação não pode ser desfeita.")
            .arg(av->nome),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (resp != QMessageBox::Yes)
        return;

    if (!m_avaliacoes.remover(av->id)) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_avaliacoes.ultimoErro());
        return;
    }
    recarregarModelo();
}

void NotasPage::moverAvaliacao(int delta)
{
    const int id = avaliacaoAtualId();
    if (id == 0)
        return;
    // Só troca com vizinhas do mesmo período; nas pontas simplesmente não faz nada.
    if (m_avaliacoes.mover(id, delta))
        recarregarModelo(id);
}

// ============================================================================
// Copiar / colar / limpar (compatível com o Excel: texto separado por tabulação)
// ============================================================================

void NotasPage::copiarSelecao()
{
    const QModelIndexList sel = m_tabela->selectionModel()->selectedIndexes();
    if (sel.isEmpty())
        return;

    int r0 = INT_MAX, r1 = -1, c0 = INT_MAX, c1 = -1;
    for (const QModelIndex &i : sel) {
        r0 = qMin(r0, i.row());
        r1 = qMax(r1, i.row());
        c0 = qMin(c0, i.column());
        c1 = qMax(c1, i.column());
    }

    QStringList linhas;
    for (int r = r0; r <= r1; ++r) {
        QStringList celulas;
        for (int c = c0; c <= c1; ++c)
            celulas << m_modelo->index(r, c).data(Qt::DisplayRole).toString();
        linhas << celulas.join(QLatin1Char('\t'));
    }
    QApplication::clipboard()->setText(linhas.join(QLatin1Char('\n')));
}

void NotasPage::colarSelecao()
{
    const QModelIndex inicio = m_tabela->currentIndex();
    if (!inicio.isValid())
        return;

    QString texto = QApplication::clipboard()->text();
    if (texto.isEmpty())
        return;
    texto.remove(QLatin1Char('\r'));
    QStringList linhas = texto.split(QLatin1Char('\n'));
    if (linhas.size() > 1 && linhas.last().isEmpty())
        linhas.removeLast();  // o Excel termina o texto copiado com quebra de linha

    int colados = 0, rejeitados = 0;
    for (int i = 0; i < linhas.size(); ++i) {
        const QStringList celulas = linhas.at(i).split(QLatin1Char('\t'));
        for (int j = 0; j < celulas.size(); ++j) {
            const QModelIndex alvo = m_modelo->index(inicio.row() + i, inicio.column() + j);
            if (!alvo.isValid() || !(m_modelo->flags(alvo) & Qt::ItemIsEditable))
                continue;  // fora da planilha ou coluna não editável (aluno/média)
            if (m_modelo->setData(alvo, celulas.at(j)))
                ++colados;
            else
                ++rejeitados;
        }
    }

    if (rejeitados > 0)
        mostrarMensagem(QStringLiteral("%1 célula(s) coladas; %2 rejeitada(s) por valor inválido ou fora do máximo.")
                            .arg(colados)
                            .arg(rejeitados),
                        true);
    else
        mostrarMensagem(QStringLiteral("%1 célula(s) coladas.").arg(colados), false);
}

void NotasPage::limparSelecao()
{
    int limpas = 0;
    for (const QModelIndex &i : m_tabela->selectionModel()->selectedIndexes()) {
        if ((m_modelo->flags(i) & Qt::ItemIsEditable) && !i.data(Qt::EditRole).toString().isEmpty())
            limpas += m_modelo->setData(i, QString()) ? 1 : 0;
    }
    if (limpas > 0)
        mostrarMensagem(QStringLiteral("%1 nota(s) apagada(s).").arg(limpas), false);
}

// ============================================================================
// Excel
// ============================================================================

void NotasPage::exportarExcel()
{
    const int turmaId = turmaAtualId();
    const auto turma = m_turmas.buscar(turmaId);
    if (!turma)
        return;

    QString nomeSugerido = QStringLiteral("Notas - %1").arg(turma->nome);
    nomeSugerido.remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")));  // caracteres proibidos em nomes de arquivo

    const QString pasta = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString caminho = QFileDialog::getSaveFileName(this, QStringLiteral("Exportar notas para Excel"),
                                                   pasta + QLatin1Char('/') + nomeSugerido + QStringLiteral(".xlsx"),
                                                   QStringLiteral("Planilha Excel (*.xlsx)"));
    if (caminho.isEmpty())
        return;
    if (!caminho.endsWith(QStringLiteral(".xlsx"), Qt::CaseInsensitive))
        caminho += QStringLiteral(".xlsx");

    // Exporta a turma inteira (todos os períodos), não só o filtro da tela.
    XlsxService::DadosExportacao dados;
    dados.titulo = turma->nome;
    dados.alunos = m_alunos.listarPorTurma(turmaId, QString(), /*incluirInativos=*/false);
    dados.avaliacoes = m_avaliacoes.listarPorTurma(turmaId, 0);
    dados.notas = m_notas.listarPorTurma(turmaId);

    QString erro;
    if (!XlsxService::exportar(caminho, dados, &erro)) {
        QMessageBox::critical(this, QStringLiteral("Erro ao exportar"), erro);
        return;
    }
    mostrarMensagem(QStringLiteral("Planilha exportada com sucesso."), false);
}

void NotasPage::importarExcel()
{
    const int turmaId = turmaAtualId();
    if (turmaId == 0)
        return;

    const QString caminho = QFileDialog::getOpenFileName(
        this, QStringLiteral("Importar notas do Excel"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QStringLiteral("Planilha Excel (*.xlsx)"));
    if (caminho.isEmpty())
        return;

    QString erro;
    const auto planilha = XlsxService::importar(caminho, &erro);
    if (!planilha) {
        QMessageBox::critical(this, QStringLiteral("Erro ao importar"), erro);
        return;
    }

    ImportadorNotas importador(m_avaliacoes, m_alunos, m_notas);
    const PlanoImportacao plano = importador.planejar(turmaId, *planilha);

    ImportarNotasDialog dlg(m_avaliacoes.listarPorTurma(turmaId, 0), *planilha, plano, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    ResultadoImportacao resultado;
    if (!importador.executar(turmaId, *planilha, dlg.plano(), &resultado, &erro)) {
        QMessageBox::critical(this, QStringLiteral("Erro ao importar"),
                              QStringLiteral("Nada foi alterado.\n\n%1").arg(erro));
        return;
    }

    recarregarModelo();
    QString msg = QStringLiteral("Importação concluída: %1 nota(s) gravada(s), %2 avaliação(ões) criada(s)")
                      .arg(resultado.notasGravadas)
                      .arg(resultado.avaliacoesCriadas);
    if (resultado.notasIgnoradas > 0)
        msg += QStringLiteral(", %1 nota(s) ignorada(s) por estarem fora de 0–máximo").arg(resultado.notasIgnoradas);
    mostrarMensagem(msg + QLatin1Char('.'), resultado.notasIgnoradas > 0);
}
