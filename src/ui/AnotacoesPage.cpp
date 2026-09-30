#include "ui/AnotacoesPage.h"
#include "ui/ThemeManager.h"

#include "database/AlunoRepository.h"
#include "database/AnotacaoRepository.h"
#include "database/AulaRepository.h"
#include "database/Repositorios.h"
#include "database/TurmaRepository.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextList>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kAtrasoSalvarMs = 1200;  // salva 1,2 s depois da última alteração

QString dataCurta(const QString &iso)
{
    const QDateTime dt = QDateTime::fromString(iso, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return dt.isValid() ? dt.toString(QStringLiteral("dd/MM/yyyy HH:mm")) : iso;
}

}  // namespace

AnotacoesPage::AnotacoesPage(Repositorios &repos, QWidget *parent)
    : QWidget(parent),
      m_anotacoes(repos.anotacoes),
      m_turmas(repos.turmas),
      m_alunos(repos.alunos),
      m_aulas(repos.aulas)
{
    auto *raiz = new QVBoxLayout(this);
    raiz->setContentsMargins(32, 28, 32, 24);
    raiz->setSpacing(6);

    auto *titulo = new QLabel(QStringLiteral("Anotações"));
    titulo->setObjectName(QStringLiteral("pageTitle"));
    auto *subtitulo = new QLabel(QStringLiteral(
        "Escreva livremente, organize com tags e ligue a uma turma, aluno ou aula. As alterações são salvas automaticamente."));
    subtitulo->setObjectName(QStringLiteral("pageSubtitle"));
    subtitulo->setWordWrap(true);
    raiz->addWidget(titulo);
    raiz->addWidget(subtitulo);
    raiz->addSpacing(12);

    auto *divisor = new QSplitter(Qt::Horizontal);
    divisor->setChildrenCollapsible(false);

    // ---------------- Lista e filtros ----------------
    auto *painelLista = new QWidget;
    auto *ll = new QVBoxLayout(painelLista);
    ll->setContentsMargins(0, 0, 12, 0);
    ll->setSpacing(8);

    m_busca = new QLineEdit;
    m_busca->setPlaceholderText(QStringLiteral("Buscar no título e no texto…"));
    m_busca->setClearButtonEnabled(true);
    ll->addWidget(m_busca);

    auto *filtros = new QHBoxLayout;
    m_filtroTurma = new QComboBox;
    m_filtroTag = new QComboBox;
    filtros->addWidget(m_filtroTurma, 1);
    filtros->addWidget(m_filtroTag, 1);
    ll->addLayout(filtros);

    m_btnNova = new QPushButton(QStringLiteral("+ Nova anotação"));
    m_btnNova->setObjectName(QStringLiteral("primary"));
    ll->addWidget(m_btnNova);

    m_lista = new QListWidget;
    m_lista->setAlternatingRowColors(true);
    ll->addWidget(m_lista, 1);

    // ---------------- Editor ----------------
    auto *painelEditor = new QWidget;
    auto *le = new QVBoxLayout(painelEditor);
    le->setContentsMargins(12, 0, 0, 0);

    m_vazio = new QLabel(QStringLiteral("Selecione uma anotação ou clique em \"+ Nova anotação\"."));
    m_vazio->setObjectName(QStringLiteral("muted"));
    m_vazio->setAlignment(Qt::AlignCenter);
    le->addWidget(m_vazio, 1);

    m_editor = new QWidget;
    auto *ed = new QVBoxLayout(m_editor);
    ed->setContentsMargins(0, 0, 0, 0);
    ed->setSpacing(8);

    m_titulo = new QLineEdit;
    m_titulo->setPlaceholderText(QStringLiteral("Título"));
    QFont fonteTitulo = m_titulo->font();
    fonteTitulo.setPointSize(fonteTitulo.pointSize() + 3);
    fonteTitulo.setBold(true);
    m_titulo->setFont(fonteTitulo);
    ed->addWidget(m_titulo);

    // Vínculos
    auto *vinculos = new QHBoxLayout;
    m_turma = new QComboBox;
    m_aluno = new QComboBox;
    m_aula = new QComboBox;
    vinculos->addWidget(m_turma, 1);
    vinculos->addWidget(m_aluno, 1);
    vinculos->addWidget(m_aula, 1);
    ed->addLayout(vinculos);

    m_tags = new QLineEdit;
    m_tags->setPlaceholderText(QStringLiteral("Tags separadas por vírgula (ex.: reunião, recuperação, projeto)"));
    ed->addWidget(m_tags);

    // Barra de formatação
    auto *formatos = new QHBoxLayout;
    formatos->setSpacing(4);
    m_btnNegrito = criarBotao(QStringLiteral("N"), QStringLiteral("Negrito (Ctrl+B)"), true);
    m_btnItalico = criarBotao(QStringLiteral("I"), QStringLiteral("Itálico (Ctrl+I)"), true);
    m_btnSublinhado = criarBotao(QStringLiteral("S"), QStringLiteral("Sublinhado (Ctrl+U)"), true);
    m_btnMarca = criarBotao(QStringLiteral("🖍"), QStringLiteral("Marca-texto"), true);
    auto *btnMarcadores = criarBotao(QStringLiteral("•"), QStringLiteral("Lista com marcadores"), false);
    auto *btnNumerada = criarBotao(QStringLiteral("1."), QStringLiteral("Lista numerada"), false);
    auto *btnLimpar = criarBotao(QStringLiteral("⌫"), QStringLiteral("Limpar formatação da seleção"), false);
    QFont fn = m_btnNegrito->font(); fn.setBold(true); m_btnNegrito->setFont(fn);
    QFont fi = m_btnItalico->font(); fi.setItalic(true); m_btnItalico->setFont(fi);
    QFont fs = m_btnSublinhado->font(); fs.setUnderline(true); m_btnSublinhado->setFont(fs);
    for (auto *b : {m_btnNegrito, m_btnItalico, m_btnSublinhado, m_btnMarca, btnMarcadores, btnNumerada, btnLimpar})
        formatos->addWidget(b);
    formatos->addStretch(1);
    ed->addLayout(formatos);

    m_texto = new QTextEdit;
    m_texto->setAcceptRichText(true);
    m_texto->setPlaceholderText(QStringLiteral("Comece a escrever…"));
    ed->addWidget(m_texto, 1);

    auto *rodape = new QHBoxLayout;
    m_estado = new QLabel;
    m_estado->setObjectName(QStringLiteral("muted"));
    m_btnExcluir = new QPushButton(QStringLiteral("Excluir anotação"));
    m_btnExcluir->setObjectName(QStringLiteral("danger"));
    rodape->addWidget(m_estado, 1);
    rodape->addWidget(m_btnExcluir);
    ed->addLayout(rodape);
    le->addWidget(m_editor, 1);

    divisor->addWidget(painelLista);
    divisor->addWidget(painelEditor);
    divisor->setStretchFactor(0, 2);
    divisor->setStretchFactor(1, 4);
    raiz->addWidget(divisor, 1);

    m_timerSalvar = new QTimer(this);
    m_timerSalvar->setSingleShot(true);
    connect(m_timerSalvar, &QTimer::timeout, this, [this] { salvarAgora(); });

    // ---------------- Conexões ----------------
    connect(m_btnNova, &QPushButton::clicked, this, &AnotacoesPage::nova);
    connect(m_btnExcluir, &QPushButton::clicked, this, &AnotacoesPage::excluir);

    // Filtros: salvam a nota aberta e refazem a lista.
    auto refazerLista = [this] {
        if (m_carregando)
            return;
        salvarAgora();
        recarregarLista(m_id);
    };
    connect(m_busca, &QLineEdit::textChanged, this, refazerLista);
    connect(m_filtroTurma, &QComboBox::currentIndexChanged, this, refazerLista);
    connect(m_filtroTag, &QComboBox::currentIndexChanged, this, refazerLista);

    connect(m_lista, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *atual) {
        if (m_carregando)
            return;
        const int novoId = atual ? atual->data(Qt::UserRole).toInt() : 0;
        if (novoId == m_id)
            return;
        salvarAgora();  // salva a anotação que estava aberta antes de trocar
        carregar(novoId);
    });

    // Edição: qualquer mudança agenda o salvamento automático.
    connect(m_titulo, &QLineEdit::textChanged, this, &AnotacoesPage::marcarSujo);
    connect(m_tags, &QLineEdit::textChanged, this, &AnotacoesPage::marcarSujo);
    connect(m_texto, &QTextEdit::textChanged, this, &AnotacoesPage::marcarSujo);
    connect(m_turma, &QComboBox::currentIndexChanged, this, [this] {
        if (m_carregando)
            return;
        atualizarCombosDaTurma();  // aluno e aula dependem da turma
        marcarSujo();
    });
    connect(m_aluno, &QComboBox::currentIndexChanged, this, &AnotacoesPage::marcarSujo);
    connect(m_aula, &QComboBox::currentIndexChanged, this, &AnotacoesPage::marcarSujo);

    // Formatação
    connect(m_btnNegrito, &QToolButton::toggled, this, &AnotacoesPage::alternarNegrito);
    connect(m_btnItalico, &QToolButton::toggled, this, &AnotacoesPage::alternarItalico);
    connect(m_btnSublinhado, &QToolButton::toggled, this, &AnotacoesPage::alternarSublinhado);
    connect(m_btnMarca, &QToolButton::toggled, this, &AnotacoesPage::alternarMarcaTexto);
    connect(btnMarcadores, &QToolButton::clicked, this, [this] { alternarLista(QTextListFormat::ListDisc); });
    connect(btnNumerada, &QToolButton::clicked, this, [this] { alternarLista(QTextListFormat::ListDecimal); });
    connect(btnLimpar, &QToolButton::clicked, this, &AnotacoesPage::limparFormatacao);
    connect(m_texto, &QTextEdit::currentCharFormatChanged, this, [this] { atualizarBotoesDeFormato(); });

    limparEditor();
}

QToolButton *AnotacoesPage::criarBotao(const QString &texto, const QString &dica, bool marcavel)
{
    auto *b = new QToolButton;
    b->setText(texto);
    b->setToolTip(dica);
    b->setCheckable(marcavel);
    b->setFixedSize(34, 30);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

void AnotacoesPage::showEvent(QShowEvent *evento)
{
    QWidget::showEvent(evento);
    recarregarFiltros();
    recarregarLista(m_id);
}

void AnotacoesPage::hideEvent(QHideEvent *evento)
{
    salvarAgora();  // ao sair da tela, nada fica pendente
    QWidget::hideEvent(evento);
}

// ============================================================================
// Lista e filtros
// ============================================================================

void AnotacoesPage::recarregarFiltros()
{
    m_carregando = true;
    const int turmaAnterior = m_filtroTurma->currentData().toInt();
    const QString tagAnterior = m_filtroTag->currentData().toString();

    m_filtroTurma->clear();
    m_filtroTurma->addItem(QStringLiteral("Todas as turmas"), 0);
    for (const Turma &t : m_turmas.listar(false))
        m_filtroTurma->addItem(t.nome, t.id);
    const int it = m_filtroTurma->findData(turmaAnterior);
    m_filtroTurma->setCurrentIndex(it >= 0 ? it : 0);

    m_filtroTag->clear();
    m_filtroTag->addItem(QStringLiteral("Todas as tags"), QString());
    for (const QString &tag : m_anotacoes.listarTags())
        m_filtroTag->addItem(QStringLiteral("#%1").arg(tag), tag);
    const int ig = m_filtroTag->findData(tagAnterior);
    m_filtroTag->setCurrentIndex(ig >= 0 ? ig : 0);

    // Combos de vínculo do editor (turmas)
    const int turmaEditor = m_turma->currentData().toInt();
    m_turma->clear();
    m_turma->addItem(QStringLiteral("Sem turma"), 0);
    for (const Turma &t : m_turmas.listar(false))
        m_turma->addItem(t.nome, t.id);
    const int ie = m_turma->findData(turmaEditor);
    m_turma->setCurrentIndex(ie >= 0 ? ie : 0);
    m_carregando = false;
}

void AnotacoesPage::recarregarLista(int selecionarId, bool recarregarEditor)
{
    AnotacaoRepository::Filtro f;
    f.texto = m_busca->text();
    f.tag = m_filtroTag->currentData().toString();
    f.turmaId = m_filtroTurma->currentData().toInt();

    m_carregando = true;
    m_lista->clear();
    QListWidgetItem *aSelecionar = nullptr;
    for (const AnotacaoResumo &r : m_anotacoes.listar(f)) {
        QStringList detalhes = {dataCurta(r.atualizadaEm)};
        if (!r.turmaNome.isEmpty())
            detalhes << r.turmaNome;
        if (!r.alunoNome.isEmpty())
            detalhes << r.alunoNome;
        QString texto = QStringLiteral("%1\n%2").arg(r.titulo.isEmpty() ? QStringLiteral("(sem título)") : r.titulo,
                                                     detalhes.join(QStringLiteral(" · ")));
        if (!r.tags.isEmpty())
            texto += QStringLiteral("\n#") + r.tags.join(QStringLiteral(" #"));

        auto *item = new QListWidgetItem(texto);
        item->setData(Qt::UserRole, r.id);
        if (!r.trecho.isEmpty())
            item->setToolTip(r.trecho);
        m_lista->addItem(item);
        if (r.id == selecionarId)
            aSelecionar = item;
    }
    if (aSelecionar)
        m_lista->setCurrentItem(aSelecionar);
    m_carregando = false;

    if (!recarregarEditor)
        return;
    if (aSelecionar)
        carregar(selecionarId);
    else
        limparEditor();
}

// ============================================================================
// Editor
// ============================================================================

void AnotacoesPage::carregar(int anotacaoId)
{
    const auto nota = anotacaoId > 0 ? m_anotacoes.buscar(anotacaoId) : std::nullopt;
    if (!nota) {
        limparEditor();
        return;
    }

    m_carregando = true;
    m_id = nota->id;
    m_titulo->setText(nota->titulo);
    m_tags->setText(nota->tags.join(QStringLiteral(", ")));
    m_texto->setHtml(nota->conteudoHtml);
    preencherVinculos(nota->turmaId, nota->alunoId, nota->aulaId);
    m_carregando = false;

    m_editor->setVisible(true);
    m_vazio->setVisible(false);
    m_sujo = false;
    m_estado->setText(QStringLiteral("Criada em %1 · alterada em %2")
                          .arg(dataCurta(nota->criadaEm), dataCurta(nota->atualizadaEm)));
    atualizarBotoesDeFormato();
}

void AnotacoesPage::limparEditor()
{
    m_timerSalvar->stop();
    m_id = 0;
    m_sujo = false;
    m_editor->setVisible(false);
    m_vazio->setVisible(true);
}

void AnotacoesPage::preencherVinculos(int turmaId, int alunoId, int aulaId)
{
    int it = m_turma->findData(turmaId);
    if (it < 0 && turmaId > 0) {
        // Turma arquivada não está na lista: mantém o vínculo mesmo assim, para
        // que salvar a anotação não o apague sem querer.
        if (const auto t = m_turmas.buscar(turmaId)) {
            m_turma->addItem(QStringLiteral("%1 (arquivada)").arg(t->nome), turmaId);
            it = m_turma->count() - 1;
        }
    }
    m_turma->setCurrentIndex(it >= 0 ? it : 0);

    m_aluno->clear();
    m_aluno->addItem(QStringLiteral("Sem aluno"), 0);
    m_aula->clear();
    m_aula->addItem(QStringLiteral("Sem aula"), 0);

    const int turma = m_turma->currentData().toInt();
    if (turma > 0) {
        for (const Aluno &a : m_alunos.listarPorTurma(turma, QString(), true))
            m_aluno->addItem(a.nome, a.id);
        for (const Aula &a : m_aulas.listar(turma))
            m_aula->addItem(QStringLiteral("%1 — %2").arg(a.data.toString(QStringLiteral("dd/MM")),
                                                          a.tema.isEmpty() ? QStringLiteral("(sem tema)") : a.tema),
                            a.id);
    }
    const int ia = m_aluno->findData(alunoId);
    m_aluno->setCurrentIndex(ia >= 0 ? ia : 0);
    const int iu = m_aula->findData(aulaId);
    m_aula->setCurrentIndex(iu >= 0 ? iu : 0);

    m_aluno->setEnabled(turma > 0);
    m_aula->setEnabled(turma > 0);
}

void AnotacoesPage::atualizarCombosDaTurma()
{
    m_carregando = true;
    preencherVinculos(m_turma->currentData().toInt(), 0, 0);  // trocar a turma zera aluno e aula
    m_carregando = false;
}

void AnotacoesPage::marcarSujo()
{
    if (m_carregando || m_id == 0)
        return;
    m_sujo = true;
    m_estado->setText(QStringLiteral("Salvando…"));
    agendarSalvamento();
}

void AnotacoesPage::agendarSalvamento()
{
    m_timerSalvar->start(kAtrasoSalvarMs);
}

bool AnotacoesPage::salvarAgora()
{
    m_timerSalvar->stop();
    if (m_id == 0 || !m_sujo)
        return true;

    Anotacao n;
    n.id = m_id;
    n.titulo = m_titulo->text().trimmed();
    n.conteudoHtml = m_texto->toHtml();
    n.conteudoTexto = m_texto->toPlainText();  // sem formatação, para a busca
    n.turmaId = m_turma->currentData().toInt();
    n.alunoId = m_aluno->currentData().toInt();
    n.aulaId = m_aula->currentData().toInt();
    n.tags = m_tags->text().split(QLatin1Char(','), Qt::SkipEmptyParts);

    if (!m_anotacoes.atualizar(n)) {
        m_estado->setText(QStringLiteral("ERRO ao salvar: %1").arg(m_anotacoes.ultimoErro()));
        return false;
    }
    m_sujo = false;
    m_estado->setText(QStringLiteral("Salvo às %1").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));

    // Atualiza a lista lateral (título, tags) sem recarregar o editor, para não
    // tirar o cursor do lugar enquanto a pessoa escreve.
    recarregarLista(m_id, /*recarregarEditor=*/false);
    return true;
}

void AnotacoesPage::nova()
{
    salvarAgora();

    Anotacao n;
    n.titulo = QStringLiteral("Nova anotação");
    // Já nasce vinculada à turma do filtro, se houver.
    n.turmaId = m_filtroTurma->currentData().toInt();
    const int id = m_anotacoes.inserir(n);
    if (id == 0) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_anotacoes.ultimoErro());
        return;
    }

    // Limpa filtros que esconderiam a nota nova.
    m_carregando = true;
    m_busca->clear();
    m_filtroTag->setCurrentIndex(0);
    m_carregando = false;

    recarregarFiltros();
    recarregarLista(id);
    m_titulo->setFocus();
    m_titulo->selectAll();
}

void AnotacoesPage::excluir()
{
    if (m_id == 0)
        return;
    const auto resp = QMessageBox::question(this, QStringLiteral("Excluir anotação"),
                                            QStringLiteral("Excluir esta anotação? Esta ação não pode ser desfeita."),
                                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (resp != QMessageBox::Yes)
        return;

    m_timerSalvar->stop();
    m_sujo = false;
    if (!m_anotacoes.remover(m_id)) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_anotacoes.ultimoErro());
        return;
    }
    m_id = 0;
    recarregarFiltros();  // uma tag pode ter deixado de existir
    recarregarLista();
}

void AnotacoesPage::selecionarAnotacao(int anotacaoId)
{
    salvarAgora();
    recarregarFiltros();
    // Remove filtros que esconderiam a anotação.
    m_carregando = true;
    m_busca->clear();
    m_filtroTurma->setCurrentIndex(0);
    m_filtroTag->setCurrentIndex(0);
    m_carregando = false;
    recarregarLista(anotacaoId);
}

// ============================================================================
// Formatação do texto
// ============================================================================

void AnotacoesPage::alternarNegrito(bool ligado)
{
    QTextCharFormat f;
    f.setFontWeight(ligado ? QFont::Bold : QFont::Normal);
    m_texto->mergeCurrentCharFormat(f);
    m_texto->setFocus();
}

void AnotacoesPage::alternarItalico(bool ligado)
{
    QTextCharFormat f;
    f.setFontItalic(ligado);
    m_texto->mergeCurrentCharFormat(f);
    m_texto->setFocus();
}

void AnotacoesPage::alternarSublinhado(bool ligado)
{
    QTextCharFormat f;
    f.setFontUnderline(ligado);
    m_texto->mergeCurrentCharFormat(f);
    m_texto->setFocus();
}

void AnotacoesPage::alternarMarcaTexto(bool ligado)
{
    QTextCharFormat f;
    // Ocre (accent) semitransparente: legível tanto no tema claro quanto no escuro.
    f.setBackground(ligado ? QBrush(ThemeManager::comAlfa(Tokens::Id::Accent, 95)) : QBrush(Qt::NoBrush));
    m_texto->mergeCurrentCharFormat(f);
    m_texto->setFocus();
}

void AnotacoesPage::alternarLista(QTextListFormat::Style estilo)
{
    QTextCursor c = m_texto->textCursor();
    if (QTextList *lista = c.currentList()) {
        if (lista->format().style() == estilo) {
            // Já é uma lista desse tipo: tira o item da lista.
            c.beginEditBlock();
            const QTextBlock bloco = c.block();
            lista->remove(bloco);
            QTextBlockFormat bf = bloco.blockFormat();
            bf.setIndent(0);
            c.setBlockFormat(bf);
            c.endEditBlock();
            m_texto->setFocus();
            return;
        }
    }
    c.createList(estilo);
    m_texto->setFocus();
}

void AnotacoesPage::limparFormatacao()
{
    QTextCursor c = m_texto->textCursor();
    if (!c.hasSelection())
        return;
    c.setCharFormat(QTextCharFormat());
    m_texto->setFocus();
}

void AnotacoesPage::atualizarBotoesDeFormato()
{
    const QTextCharFormat f = m_texto->currentCharFormat();
    // blockSignals: só reflete o estado atual, sem reaplicar o formato.
    m_btnNegrito->blockSignals(true);
    m_btnNegrito->setChecked(f.fontWeight() >= QFont::Bold);
    m_btnNegrito->blockSignals(false);
    m_btnItalico->blockSignals(true);
    m_btnItalico->setChecked(f.fontItalic());
    m_btnItalico->blockSignals(false);
    m_btnSublinhado->blockSignals(true);
    m_btnSublinhado->setChecked(f.fontUnderline());
    m_btnSublinhado->blockSignals(false);
    m_btnMarca->blockSignals(true);
    m_btnMarca->setChecked(f.background().style() != Qt::NoBrush);
    m_btnMarca->blockSignals(false);
}
