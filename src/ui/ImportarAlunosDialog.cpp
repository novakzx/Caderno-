#include "ui/ImportarAlunosDialog.h"

#include "database/AlunoRepository.h"
#include "ui/ThemeManager.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QString textoDaSituacao(const LinhaImportacaoAluno &l)
{
    QString texto;
    switch (l.situacao) {
    case LinhaImportacaoAluno::Situacao::Nova:
        texto = QStringLiteral("Novo");
        break;
    case LinhaImportacaoAluno::Situacao::JaExiste:
        texto = QStringLiteral("Já está na turma");
        break;
    case LinhaImportacaoAluno::Situacao::RepetidaNoArquivo:
        texto = QStringLiteral("Repetido na lista");
        break;
    case LinhaImportacaoAluno::Situacao::Invalida:
        texto = QStringLiteral("Inválido");
        break;
    }
    if (!l.detalhe.isEmpty())
        texto += QStringLiteral(" — ") + l.detalhe;
    return texto;
}

}  // namespace

ImportarAlunosDialog::ImportarAlunosDialog(AlunoRepository &alunos, int turmaId, const QString &turmaNome, QWidget *parent)
    : QDialog(parent), m_alunos(alunos), m_importador(alunos), m_turmaId(turmaId)
{
    setWindowTitle(QStringLiteral("Importar alunos"));
    setMinimumSize(860, 640);
    resize(900, 660);

    auto *titulo = new QLabel(QStringLiteral("Importar alunos para %1").arg(turmaNome));
    titulo->setObjectName(QStringLiteral("sectionTitle"));
    titulo->setTextFormat(Qt::PlainText);  // o nome da turma vem do usuário
    auto *dica = new QLabel(QStringLiteral("Aceita arquivos .csv, .txt e .xlsx, ou uma lista colada. A primeira linha deve ter "
                                           "os títulos das colunas: Nome (obrigatório), Matrícula, E-mail e Nascimento. "
                                           "Uma lista só com nomes, um por linha, também funciona. Nada é gravado antes de "
                                           "você confirmar."));
    dica->setObjectName(QStringLiteral("muted"));
    dica->setWordWrap(true);

    // --- Origem: arquivo ou texto colado ---
    m_origem = new QTabWidget;

    auto *abaArquivo = new QWidget;
    auto *la = new QVBoxLayout(abaArquivo);
    la->setContentsMargins(0, 12, 0, 0);
    auto *botoesArquivo = new QHBoxLayout;
    auto *escolher = new QPushButton(QStringLiteral("Escolher arquivo…"));
    escolher->setObjectName(QStringLiteral("primary"));
    ThemeManager::iconeNoBotao(escolher, QStringLiteral("subir"), Tokens::Id::OnPrimary);
    auto *modelo = new QPushButton(QStringLiteral("Baixar modelo (CSV)"));
    ThemeManager::iconeNoBotao(modelo, QStringLiteral("baixar"));
    botoesArquivo->addWidget(escolher);
    botoesArquivo->addWidget(modelo);
    botoesArquivo->addStretch(1);
    m_arquivo = new QLabel(QStringLiteral("Nenhum arquivo escolhido."));
    m_arquivo->setObjectName(QStringLiteral("muted"));
    m_arquivo->setTextFormat(Qt::PlainText);  // nome de arquivo: nunca como HTML
    la->addLayout(botoesArquivo);
    la->addWidget(m_arquivo);
    m_origem->addTab(abaArquivo, QStringLiteral("Arquivo"));

    auto *abaColar = new QWidget;
    auto *lc = new QVBoxLayout(abaColar);
    lc->setContentsMargins(0, 12, 0, 0);
    m_colado = new QPlainTextEdit;
    m_colado->setFixedHeight(96);
    m_colado->setPlaceholderText(QStringLiteral("Cole aqui os nomes (um por linha) ou as linhas copiadas do Excel, "
                                                "com a linha de títulos."));
    auto *analisar = new QPushButton(QStringLiteral("Analisar o texto colado"));
    lc->addWidget(m_colado);
    lc->addWidget(analisar, 0, Qt::AlignLeft);
    m_origem->addTab(abaColar, QStringLiteral("Colar lista"));

    // --- Pré-visualização ---
    m_resumo = new QLabel;
    m_resumo->setWordWrap(true);
    m_resumo->setTextFormat(Qt::PlainText);

    m_previa = new QTableWidget(0, 6);
    m_previa->setHorizontalHeaderLabels({QStringLiteral("Linha"), QStringLiteral("Nome"), QStringLiteral("Matrícula"),
                                         QStringLiteral("E-mail"), QStringLiteral("Nascimento"), QStringLiteral("Situação")});
    m_previa->setSelectionMode(QAbstractItemView::NoSelection);
    m_previa->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_previa->setAlternatingRowColors(true);
    m_previa->setShowGrid(false);
    m_previa->verticalHeader()->setVisible(false);
    m_previa->verticalHeader()->setDefaultSectionSize(30);
    m_previa->horizontalHeader()->setHighlightSections(false);
    m_previa->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_previa->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_previa->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_previa->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_previa->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_previa->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);

    auto *botoes = new QDialogButtonBox;
    m_botaoImportar = botoes->addButton(QStringLiteral("Importar"), QDialogButtonBox::AcceptRole);
    m_botaoImportar->setObjectName(QStringLiteral("primary"));
    m_botaoImportar->setEnabled(false);
    botoes->addButton(QStringLiteral("Cancelar"), QDialogButtonBox::RejectRole);
    connect(botoes, &QDialogButtonBox::accepted, this, &ImportarAlunosDialog::importar);
    connect(botoes, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(10);
    layout->addWidget(titulo);
    layout->addWidget(dica);
    layout->addWidget(m_origem);
    layout->addWidget(m_resumo);
    layout->addWidget(m_previa, 1);
    layout->addWidget(botoes);

    connect(escolher, &QPushButton::clicked, this, &ImportarAlunosDialog::escolherArquivo);
    connect(modelo, &QPushButton::clicked, this, &ImportarAlunosDialog::baixarModelo);
    connect(analisar, &QPushButton::clicked, this, &ImportarAlunosDialog::analisarTextoColado);
}

void ImportarAlunosDialog::escolherArquivo()
{
    const QString caminho = QFileDialog::getOpenFileName(
        this, QStringLiteral("Escolher a lista de alunos"), QString(),
        QStringLiteral("Listas de alunos (*.csv *.txt *.tsv *.xlsx);;Todos os arquivos (*)"));
    if (caminho.isEmpty())
        return;
    m_arquivo->setText(QDir::toNativeSeparators(caminho));

    QString erro;
    const auto tabela = ImportadorAlunos::tabelaDeArquivo(caminho, &erro);
    mostrarTabela(tabela, erro);
}

void ImportarAlunosDialog::analisarTextoColado()
{
    QString erro;
    const auto tabela = ImportadorAlunos::tabelaDeTexto(m_colado->toPlainText().toUtf8(), &erro);
    mostrarTabela(tabela, erro);
}

void ImportarAlunosDialog::mostrarTabela(const std::optional<QList<QStringList>> &tabela, const QString &erro)
{
    m_tabela.clear();
    m_plano = PlanoImportacaoAlunos();
    if (!tabela) {
        m_plano.erro = erro;
    } else {
        m_tabela = *tabela;
        m_plano = m_importador.planejar(m_turmaId, m_tabela);
    }
    preencherPrevia();
}

void ImportarAlunosDialog::preencherPrevia()
{
    m_previa->setRowCount(0);
    m_botaoImportar->setEnabled(false);
    m_botaoImportar->setText(QStringLiteral("Importar"));

    if (!m_plano.erro.isEmpty()) {
        m_resumo->setText(m_plano.erro);
        ThemeManager::definirEstado(m_resumo, ThemeManager::Estado::Erro);
        return;
    }

    const QColor mudo = ThemeManager::cor(Tokens::Id::InkMuted);
    for (const LinhaImportacaoAluno &l : m_plano.linhas) {
        const int linha = m_previa->rowCount();
        m_previa->insertRow(linha);
        const bool ignorada = l.situacao != LinhaImportacaoAluno::Situacao::Nova;
        const QStringList celulas = {
            QString::number(l.linhaOrigem),
            l.aluno.nome,
            l.aluno.matricula,
            l.aluno.email,
            l.aluno.dataNascimento.isValid() ? l.aluno.dataNascimento.toString(QStringLiteral("dd/MM/yyyy")) : QString(),
            textoDaSituacao(l),
        };
        for (int c = 0; c < celulas.size(); ++c) {
            auto *item = new QTableWidgetItem(celulas.at(c));  // texto simples: nunca interpretado como HTML
            item->setToolTip(celulas.at(c));
            if (ignorada)
                item->setForeground(mudo);  // linhas ignoradas ficam discretas (a situação também vem escrita)
            m_previa->setItem(linha, c, item);
        }
    }

    using S = LinhaImportacaoAluno::Situacao;
    QStringList partes;
    partes << (m_plano.novos() == 1 ? QStringLiteral("1 aluno novo") : QStringLiteral("%1 alunos novos").arg(m_plano.novos()));
    if (const int n = m_plano.total(S::JaExiste))
        partes << QStringLiteral("%1 já na turma").arg(n);
    if (const int n = m_plano.total(S::RepetidaNoArquivo))
        partes << QStringLiteral("%1 repetido(s) na lista").arg(n);
    if (const int n = m_plano.total(S::Invalida))
        partes << QStringLiteral("%1 inválido(s)").arg(n);
    m_resumo->setText(partes.join(QStringLiteral(" · ")));
    ThemeManager::definirEstado(m_resumo, ThemeManager::Estado::Neutro);

    if (m_plano.novos() > 0) {
        m_botaoImportar->setEnabled(true);
        m_botaoImportar->setText(m_plano.novos() == 1 ? QStringLiteral("Importar 1 aluno")
                                                      : QStringLiteral("Importar %1 alunos").arg(m_plano.novos()));
    }
}

void ImportarAlunosDialog::baixarModelo()
{
    const QString caminho = QFileDialog::getSaveFileName(this, QStringLiteral("Salvar o modelo de lista de alunos"),
                                                         QStringLiteral("modelo-alunos.csv"),
                                                         QStringLiteral("Arquivo CSV (*.csv)"));
    if (caminho.isEmpty())
        return;
    QFile arquivo(caminho);
    if (!arquivo.open(QIODevice::WriteOnly) || arquivo.write(ImportadorAlunos::modeloCsv()) < 0) {
        QMessageBox::critical(this, QStringLiteral("Não foi possível salvar"),
                              QStringLiteral("Não foi possível gravar o arquivo: %1").arg(arquivo.errorString()));
        return;
    }
    QMessageBox::information(this, QStringLiteral("Modelo salvo"),
                             QStringLiteral("Preencha o modelo no Excel (uma linha por aluno) e use \"Escolher arquivo…\"."));
}

void ImportarAlunosDialog::importar()
{
    if (m_plano.novos() == 0)
        return;
    int criados = 0;
    QString erro;
    if (!m_importador.executar(m_turmaId, m_plano, &criados, &erro)) {
        QMessageBox::critical(this, QStringLiteral("Não foi possível importar"),
                              QStringLiteral("Nenhum aluno foi gravado.\n\n%1").arg(erro));
        return;
    }
    m_criados = criados;
    accept();
}
