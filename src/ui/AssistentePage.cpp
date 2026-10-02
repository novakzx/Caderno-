#include "ui/AssistentePage.h"

#include "database/AnotacaoRepository.h"
#include "database/AulaRepository.h"
#include "database/Repositorios.h"
#include "database/TurmaRepository.h"
#include "services/IaService.h"
#include "ui/ThemeManager.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTextCursor>
#include <QVBoxLayout>

using IaPrompts::Tarefa;

namespace {

constexpr int kLarguraDoFormulario = 360;

QLabel *rotulo(const QString &texto)
{
    auto *l = new QLabel(texto);
    l->setObjectName(QStringLiteral("muted"));
    return l;
}

}  // namespace

AssistentePage::AssistentePage(Repositorios &repos, QWidget *parent)
    : QWidget(parent), m_turmas(repos.turmas), m_anotacoes(repos.anotacoes), m_aulas(repos.aulas), m_ia(new IaService(this))
{
    auto *raiz = new QVBoxLayout(this);
    raiz->setContentsMargins(32, 28, 32, 24);
    raiz->setSpacing(6);

    auto *titulo = new QLabel(QStringLiteral("Assistente de IA"));
    titulo->setObjectName(QStringLiteral("pageTitle"));
    auto *subtitulo = new QLabel(QStringLiteral("Planos de aula, questões, atividades e comunicados em segundos. Revise sempre antes de usar."));
    subtitulo->setObjectName(QStringLiteral("pageSubtitle"));
    raiz->addWidget(titulo);
    raiz->addWidget(subtitulo);
    raiz->addSpacing(8);

    // --- Aviso de "IA não configurada" ---
    m_aviso = new QFrame;
    m_aviso->setObjectName(QStringLiteral("card"));
    auto *la = new QHBoxLayout(m_aviso);
    la->setContentsMargins(16, 12, 16, 12);
    m_textoAviso = new QLabel;
    m_textoAviso->setWordWrap(true);
    m_textoAviso->setTextFormat(Qt::PlainText);
    auto *configurar = new QPushButton(QStringLiteral("Configurar"));
    configurar->setObjectName(QStringLiteral("primary"));
    la->addWidget(m_textoAviso, 1);
    la->addWidget(configurar, 0, Qt::AlignVCenter);
    raiz->addWidget(m_aviso);
    raiz->addSpacing(4);

    auto *corpo = new QHBoxLayout;
    corpo->setSpacing(20);

    // ------------------------- Coluna do formulário -------------------------
    auto *colunaForm = new QFrame;
    colunaForm->setObjectName(QStringLiteral("card"));
    colunaForm->setFixedWidth(kLarguraDoFormulario);
    auto *lf = new QVBoxLayout(colunaForm);
    lf->setContentsMargins(18, 16, 18, 16);
    lf->setSpacing(6);

    m_tarefa = new QComboBox;
    for (Tarefa t : {Tarefa::PlanoDeAula, Tarefa::Questoes, Tarefa::Atividade, Tarefa::Comunicado, Tarefa::AdaptarTexto, Tarefa::Livre})
        m_tarefa->addItem(IaPrompts::rotulo(t), static_cast<int>(t));
    lf->addWidget(rotulo(QStringLiteral("O que você quer criar?")));
    lf->addWidget(m_tarefa);

    m_turma = new QComboBox;
    m_turma->addItem(QStringLiteral("Sem turma"), 0);
    lf->addWidget(rotulo(QStringLiteral("Turma (opcional, só preenche os campos)")));
    lf->addWidget(m_turma);

    m_disciplina = new QLineEdit;
    m_disciplina->setMaxLength(120);
    m_disciplina->setPlaceholderText(QStringLiteral("ex.: Matemática"));
    auto *rotDisciplina = rotulo(QStringLiteral("Disciplina"));
    lf->addWidget(rotDisciplina);
    lf->addWidget(m_disciplina);
    m_camposDisciplina = {rotDisciplina, m_disciplina};

    m_serie = new QLineEdit;
    m_serie->setMaxLength(120);
    m_serie->setPlaceholderText(QStringLiteral("ex.: 8º ano do Ensino Fundamental"));
    lf->addWidget(rotulo(QStringLiteral("Série / público")));
    lf->addWidget(m_serie);

    m_tema = new QLineEdit;
    m_tema->setMaxLength(200);
    m_tema->setPlaceholderText(QStringLiteral("ex.: Equações do 1º grau"));
    auto *rotTema = rotulo(QStringLiteral("Tema"));
    lf->addWidget(rotTema);
    lf->addWidget(m_tema);
    m_camposTema = {rotTema, m_tema};

    m_duracao = new QComboBox;
    m_duracao->setEditable(true);
    m_duracao->addItems({QStringLiteral("40 minutos"), QStringLiteral("50 minutos"), QStringLiteral("90 minutos (aula dupla)")});
    m_duracao->setCurrentText(QStringLiteral("50 minutos"));
    auto *rotDuracao = rotulo(QStringLiteral("Duração"));
    lf->addWidget(rotDuracao);
    lf->addWidget(m_duracao);
    m_camposDuracao = {rotDuracao, m_duracao};

    m_quantidade = new QSpinBox;
    m_quantidade->setRange(1, 20);
    m_quantidade->setValue(5);
    auto *rotQuantidade = rotulo(QStringLiteral("Quantidade"));
    lf->addWidget(rotQuantidade);
    lf->addWidget(m_quantidade);
    m_camposQuantidade = {rotQuantidade, m_quantidade};

    m_rotuloDetalhes = rotulo(QString());
    m_rotuloDetalhes->setWordWrap(true);
    m_detalhes = new QPlainTextEdit;
    m_detalhes->setMinimumHeight(90);
    lf->addWidget(m_rotuloDetalhes);
    lf->addWidget(m_detalhes, 1);

    m_botaoGerar = new QPushButton(QStringLiteral("Gerar"));
    m_botaoGerar->setObjectName(QStringLiteral("primary"));
    ThemeManager::iconeNoBotao(m_botaoGerar, QStringLiteral("assistente"), Tokens::Id::OnPrimary);
    lf->addSpacing(6);
    lf->addWidget(m_botaoGerar);
    corpo->addWidget(colunaForm);

    // ------------------------- Coluna do resultado -------------------------
    auto *colunaResultado = new QVBoxLayout;
    colunaResultado->setSpacing(8);
    auto *cabecalho = new QHBoxLayout;
    auto *tituloResultado = new QLabel(QStringLiteral("Resposta"));
    tituloResultado->setObjectName(QStringLiteral("sectionTitle"));
    m_status = new QLabel;
    m_status->setObjectName(QStringLiteral("muted"));
    m_status->setTextFormat(Qt::PlainText);
    cabecalho->addWidget(tituloResultado);
    cabecalho->addStretch(1);
    cabecalho->addWidget(m_status);
    colunaResultado->addLayout(cabecalho);

    m_progresso = new QProgressBar;
    m_progresso->setRange(0, 0);  // indeterminado: "trabalhando"
    m_progresso->setTextVisible(false);
    m_progresso->setFixedHeight(4);
    m_progresso->hide();
    colunaResultado->addWidget(m_progresso);

    m_resultado = new QPlainTextEdit;  // texto simples: a resposta da IA nunca é interpretada como HTML
    m_resultado->setPlaceholderText(QStringLiteral("A resposta aparece aqui, aos poucos. Você pode editar o texto antes de copiar ou salvar."));
    colunaResultado->addWidget(m_resultado, 1);

    auto *botoes = new QHBoxLayout;
    m_botaoCopiar = new QPushButton(QStringLiteral("Copiar"));
    ThemeManager::iconeNoBotao(m_botaoCopiar, QStringLiteral("copiar"));
    m_botaoAnotacao = new QPushButton(QStringLiteral("Salvar como anotação"));
    ThemeManager::iconeNoBotao(m_botaoAnotacao, QStringLiteral("anotacoes"));
    m_botaoAula = new QPushButton(QStringLiteral("Criar plano de aula"));
    ThemeManager::iconeNoBotao(m_botaoAula, QStringLiteral("aulas"));
    m_botaoLimpar = new QPushButton(QStringLiteral("Limpar"));
    botoes->addWidget(m_botaoCopiar);
    botoes->addWidget(m_botaoAnotacao);
    botoes->addWidget(m_botaoAula);
    botoes->addStretch(1);
    botoes->addWidget(m_botaoLimpar);
    colunaResultado->addLayout(botoes);

    auto *nota = rotulo(IaPrompts::avisoDeRevisao() + QStringLiteral(" O texto que você escreve aqui é enviado ao Cloudflare; não inclua nomes de alunos."));
    nota->setWordWrap(true);
    colunaResultado->addWidget(nota);
    corpo->addLayout(colunaResultado, 1);
    raiz->addLayout(corpo, 1);

    // ------------------------------ Conexões ------------------------------
    connect(configurar, &QPushButton::clicked, this, &AssistentePage::configurarSolicitado);
    connect(m_tarefa, &QComboBox::currentIndexChanged, this, [this] { atualizarCampos(); });
    connect(m_turma, &QComboBox::currentIndexChanged, this, [this] { turmaMudou(); });
    connect(m_botaoGerar, &QPushButton::clicked, this, [this] { m_ocupado ? parar() : gerar(); });
    connect(m_botaoCopiar, &QPushButton::clicked, this, &AssistentePage::copiar);
    connect(m_botaoAnotacao, &QPushButton::clicked, this, &AssistentePage::salvarComoAnotacao);
    connect(m_botaoAula, &QPushButton::clicked, this, &AssistentePage::criarPlanoDeAula);
    connect(m_botaoLimpar, &QPushButton::clicked, this, [this] {
        m_resultado->clear();
        m_status->clear();
    });
    connect(m_resultado, &QPlainTextEdit::textChanged, this, [this] { definirOcupado(m_ocupado); });
    connect(m_ia, &IaService::trecho, this, [this](const QString &texto) {
        m_resultado->moveCursor(QTextCursor::End);
        m_resultado->insertPlainText(texto);
        m_resultado->ensureCursorVisible();
    });
    connect(m_ia, &IaService::concluido, this, &AssistentePage::aoConcluir);

    atualizarCampos();
    definirOcupado(false);
}

void AssistentePage::showEvent(QShowEvent *evento)
{
    QWidget::showEvent(evento);
    // Recarrega a lista de turmas (podem ter mudado) mantendo a escolha.
    const int atual = turmaEscolhida();
    m_turma->blockSignals(true);
    m_turma->clear();
    m_turma->addItem(QStringLiteral("Sem turma"), 0);
    for (const Turma &t : m_turmas.listar(false)) {
        QString nome = t.nome;
        if (!t.disciplina.isEmpty())
            nome += QStringLiteral(" — ") + t.disciplina;
        m_turma->addItem(nome, t.id);
    }
    m_turma->setCurrentIndex(qMax(0, m_turma->findData(atual)));
    m_turma->blockSignals(false);
    atualizarAviso();
}

void AssistentePage::hideEvent(QHideEvent *evento)
{
    QWidget::hideEvent(evento);
    if (m_ocupado)
        m_ia->cancelar();  // sair da tela interrompe a geração (e a cota não é gasta à toa)
}

void AssistentePage::atualizarAviso()
{
    const ConfigIa c = IaConfig::carregar();
    const bool pronto = c.completa();
    m_aviso->setVisible(!pronto);
    if (!pronto)
        m_textoAviso->setText(QStringLiteral("A IA ainda não está configurada. Ela usa a sua conta gratuita do Cloudflare: informe o Account ID e "
                                             "um token nas Configurações (o passo a passo está em docs/GUIA-IA.md)."));
    definirOcupado(m_ocupado);
}

void AssistentePage::atualizarCampos()
{
    const Tarefa t = static_cast<Tarefa>(m_tarefa->currentData().toInt());
    for (QWidget *w : std::as_const(m_camposDisciplina)) w->setVisible(IaPrompts::usaDisciplina(t));
    for (QWidget *w : std::as_const(m_camposTema)) w->setVisible(IaPrompts::usaTema(t));
    for (QWidget *w : std::as_const(m_camposDuracao)) w->setVisible(IaPrompts::usaDuracao(t));
    for (QWidget *w : std::as_const(m_camposQuantidade)) w->setVisible(IaPrompts::usaQuantidade(t));
    m_rotuloDetalhes->setText(IaPrompts::dicaDosDetalhes(t));
    m_botaoAula->setVisible(t == Tarefa::PlanoDeAula);
}

int AssistentePage::turmaEscolhida() const
{
    return m_turma->currentData().toInt();
}

void AssistentePage::turmaMudou()
{
    const auto turma = m_turmas.buscar(turmaEscolhida());
    if (!turma)
        return;
    if (!turma->disciplina.isEmpty())
        m_disciplina->setText(turma->disciplina);
    m_serie->setText(turma->nome);
}

IaPrompts::Pedido AssistentePage::pedidoAtual() const
{
    IaPrompts::Pedido p;
    p.tarefa = static_cast<Tarefa>(m_tarefa->currentData().toInt());
    p.disciplina = m_disciplina->text();
    p.serie = m_serie->text();
    p.tema = m_tema->text();
    p.duracao = m_duracao->currentText();
    p.detalhes = m_detalhes->toPlainText();
    p.quantidade = m_quantidade->value();
    return p;
}

bool AssistentePage::confirmarEnvio()
{
    if (IaConfig::consentimentoDado())
        return true;
    QMessageBox caixa(this);
    caixa.setIcon(QMessageBox::Information);
    caixa.setWindowTitle(QStringLiteral("Antes de usar a IA"));
    caixa.setText(QStringLiteral("O texto que você escreve neste assistente será enviado pela internet ao Cloudflare (Workers AI) "
                                 "para gerar a resposta."));
    caixa.setInformativeText(QStringLiteral("• O Caderno+ não envia nada por conta própria: só o que você digita aqui, quando clica em Gerar.\n"
                                            "• Não digite nomes nem dados pessoais de alunos.\n"
                                            "• A resposta pode conter erros: revise antes de usar com a turma."));
    auto *continuar = caixa.addButton(QStringLiteral("Entendi, continuar"), QMessageBox::AcceptRole);
    caixa.addButton(QStringLiteral("Cancelar"), QMessageBox::RejectRole);
    caixa.exec();
    if (caixa.clickedButton() != continuar)
        return false;
    IaConfig::definirConsentimento(true);
    return true;
}

void AssistentePage::gerar()
{
    const ConfigIa config = IaConfig::carregar();
    if (!config.completa()) {
        atualizarAviso();
        emit configurarSolicitado();
        return;
    }
    const IaPrompts::Pedido pedido = pedidoAtual();
    const bool precisaDeTexto = pedido.tarefa == Tarefa::Livre || pedido.tarefa == Tarefa::Comunicado || pedido.tarefa == Tarefa::AdaptarTexto;
    if (precisaDeTexto && pedido.detalhes.trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Falta o texto"),
                                 QStringLiteral("Escreva no campo de baixo o que você quer (ou o texto a adaptar)."));
        m_detalhes->setFocus();
        return;
    }
    if (!precisaDeTexto && IaPrompts::usaTema(pedido.tarefa) && pedido.tema.trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Falta o tema"), QStringLiteral("Informe o tema da aula."));
        m_tema->setFocus();
        return;
    }
    if (!m_resultado->toPlainText().trimmed().isEmpty()
        && QMessageBox::question(this, QStringLiteral("Substituir a resposta?"),
                                 QStringLiteral("A resposta atual será apagada. Se quiser guardá-la, salve ou copie antes."),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    if (!confirmarEnvio())
        return;

    m_resultado->clear();
    definirOcupado(true);
    m_status->setText(QStringLiteral("Gerando…"));
    m_ia->enviar(config, IaPrompts::montar(pedido), 1400);
}

void AssistentePage::parar()
{
    m_ia->cancelar();
}

void AssistentePage::aoConcluir(const ResultadoIa &r)
{
    definirOcupado(false);
    if (r.cancelado) {
        m_status->setText(QStringLiteral("Interrompido"));
        return;
    }
    if (!r.ok) {
        m_status->setText(QStringLiteral("Não foi possível gerar"));
        QMessageBox::warning(this, QStringLiteral("Assistente de IA"), r.erro);
        return;
    }
    m_status->setText(QStringLiteral("Pronto. Revise antes de usar."));
}

void AssistentePage::definirOcupado(bool ocupado)
{
    m_ocupado = ocupado;
    m_progresso->setVisible(ocupado);
    m_botaoGerar->setText(ocupado ? QStringLiteral("Parar") : QStringLiteral("Gerar"));
    const bool temTexto = !m_resultado->toPlainText().trimmed().isEmpty();
    m_botaoCopiar->setEnabled(temTexto && !ocupado);
    m_botaoAnotacao->setEnabled(temTexto && !ocupado);
    m_botaoAula->setEnabled(temTexto && !ocupado && turmaEscolhida() != 0);
    m_botaoAula->setToolTip(turmaEscolhida() == 0 ? QStringLiteral("Escolha uma turma para criar o plano de aula") : QString());
    m_botaoLimpar->setEnabled(temTexto && !ocupado);
    m_resultado->setReadOnly(ocupado);
}

QString AssistentePage::tituloSugerido() const
{
    const Tarefa t = static_cast<Tarefa>(m_tarefa->currentData().toInt());
    QString titulo = QStringLiteral("IA — %1").arg(IaPrompts::rotulo(t));
    const QString tema = IaPrompts::limpar(m_tema->text(), 80);
    if (!tema.isEmpty() && IaPrompts::usaTema(t))
        titulo += QStringLiteral(": ") + tema;
    return titulo;
}

void AssistentePage::copiar()
{
    QApplication::clipboard()->setText(m_resultado->toPlainText());
    m_status->setText(QStringLiteral("Copiado."));
}

void AssistentePage::salvarComoAnotacao()
{
    const QString texto = m_resultado->toPlainText().trimmed();
    if (texto.isEmpty())
        return;
    Anotacao a;
    a.titulo = tituloSugerido();
    a.conteudoTexto = texto + QStringLiteral("\n\n") + IaPrompts::avisoDeRevisao();
    // O texto da IA entra como TEXTO: escapado antes de virar HTML do editor (nada de tags vindas do modelo).
    a.conteudoHtml = QStringLiteral("<p>%1</p><p><i>%2</i></p>")
                         .arg(texto.toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br>")), IaPrompts::avisoDeRevisao().toHtmlEscaped());
    a.turmaId = turmaEscolhida();
    a.tags = {QStringLiteral("ia")};
    if (m_anotacoes.inserir(a) == 0) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_anotacoes.ultimoErro());
        return;
    }
    m_status->setText(QStringLiteral("Salvo em Anotações (tag \"ia\")."));
}

void AssistentePage::criarPlanoDeAula()
{
    const int turmaId = turmaEscolhida();
    const QString texto = m_resultado->toPlainText().trimmed();
    if (turmaId == 0 || texto.isEmpty())
        return;

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Criar plano de aula"));
    auto *form = new QFormLayout;
    auto *data = new QDateEdit(QDate::currentDate());
    data->setCalendarPopup(true);
    data->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    auto *tema = new QLineEdit(IaPrompts::limpar(m_tema->text(), 200));
    tema->setMaxLength(200);
    form->addRow(QStringLiteral("Data da aula"), data);
    form->addRow(QStringLiteral("Tema"), tema);
    auto *botoes = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botoes->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Criar"));
    botoes->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("primary"));
    botoes->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
    connect(botoes, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(botoes, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    auto *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(20, 20, 20, 16);
    lay->setSpacing(12);
    lay->addWidget(new QLabel(QStringLiteral("O plano gerado vai para o campo \"Observações\" da aula.")));
    lay->addLayout(form);
    lay->addWidget(botoes);
    if (dlg.exec() != QDialog::Accepted)
        return;

    Aula aula;
    aula.turmaId = turmaId;
    aula.data = data->date();
    aula.tema = tema->text().trimmed();
    aula.observacoes = texto + QStringLiteral("\n\n") + IaPrompts::avisoDeRevisao();
    if (m_aulas.inserir(aula) == 0) {
        QMessageBox::critical(this, QStringLiteral("Erro"), m_aulas.ultimoErro());
        return;
    }
    m_status->setText(QStringLiteral("Plano criado em Aulas (%1).").arg(aula.data.toString(QStringLiteral("dd/MM/yyyy"))));
}
