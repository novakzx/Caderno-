#include "ui/RelatoriosPage.h"

#include "core/FrequenciaUtil.h"
#include "database/AlunoRepository.h"
#include "database/Repositorios.h"
#include "database/TurmaRepository.h"
#include "services/DesempenhoService.h"
#include "services/RelatorioPdf.h"
#include "ui/ChartWidget.h"

#include <QComboBox>
#include <QDate>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>

namespace {

QString numero(double v, int casas = 2)
{
    return QString::number(v, 'f', casas).replace(QLatin1Char('.'), QLatin1Char(','));
}

// Remove caracteres proibidos em nomes de arquivo.
QString nomeDeArquivoSeguro(QString nome)
{
    nome.remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")));
    return nome.trimmed();
}

}  // namespace

RelatoriosPage::RelatoriosPage(Repositorios &repos, QWidget *parent)
    : QWidget(parent), m_turmas(repos.turmas), m_alunos(repos.alunos), m_servico(new DesempenhoService(repos))
{
    auto *raiz = new QVBoxLayout(this);
    raiz->setContentsMargins(32, 28, 32, 24);
    raiz->setSpacing(8);

    auto *titulo = new QLabel(QStringLiteral("Relatórios e desempenho"));
    titulo->setObjectName(QStringLiteral("pageTitle"));
    auto *subtitulo = new QLabel(QStringLiteral("Gráficos da turma e relatórios em PDF (boletim, frequência e ficha do aluno)."));
    subtitulo->setObjectName(QStringLiteral("pageSubtitle"));
    raiz->addWidget(titulo);
    raiz->addWidget(subtitulo);
    raiz->addSpacing(8);

    // Barra de seleção
    auto *barra = new QHBoxLayout;
    barra->setSpacing(10);
    m_comboTurma = new QComboBox;
    m_comboTurma->setMinimumWidth(220);
    m_comboPeriodo = new QComboBox;
    m_comboPeriodo->addItem(QStringLiteral("Todos os períodos"), 0);
    for (int p = 1; p <= 4; ++p)
        m_comboPeriodo->addItem(QStringLiteral("%1º período").arg(p), p);
    m_comboGrafico = new QComboBox;
    m_comboGrafico->addItem(QStringLiteral("Média por aluno"), MediaPorAluno);
    m_comboGrafico->addItem(QStringLiteral("Média da turma por avaliação"), MediaPorAvaliacao);
    m_comboGrafico->addItem(QStringLiteral("Distribuição das médias"), DistribuicaoDeMedias);
    m_comboGrafico->addItem(QStringLiteral("Frequência por aluno"), FrequenciaPorAluno);
    m_comboGrafico->addItem(QStringLiteral("Evolução de um aluno"), EvolucaoDoAluno);
    m_comboAluno = new QComboBox;
    m_comboAluno->setMinimumWidth(200);
    barra->addWidget(m_comboTurma);
    barra->addWidget(m_comboPeriodo);
    barra->addWidget(m_comboGrafico);
    barra->addWidget(m_comboAluno);
    barra->addStretch(1);
    raiz->addLayout(barra);

    m_grafico = new ChartWidget;
    raiz->addWidget(m_grafico, 1);

    m_resumo = new QLabel;
    m_resumo->setObjectName(QStringLiteral("muted"));
    m_resumo->setWordWrap(true);
    raiz->addWidget(m_resumo);

    // Botões de exportação
    auto *botoes = new QHBoxLayout;
    auto *btnPng = new QPushButton(QStringLiteral("🖼 Salvar gráfico (PNG)"));
    auto *btnBoletim = new QPushButton(QStringLiteral("📄 Boletim da turma (PDF)"));
    auto *btnFrequencia = new QPushButton(QStringLiteral("📄 Frequência da turma (PDF)"));
    auto *btnFicha = new QPushButton(QStringLiteral("📄 Ficha do aluno (PDF)"));
    btnBoletim->setObjectName(QStringLiteral("primary"));
    botoes->addWidget(btnPng);
    botoes->addStretch(1);
    botoes->addWidget(btnBoletim);
    botoes->addWidget(btnFrequencia);
    botoes->addWidget(btnFicha);
    raiz->addLayout(botoes);

    auto atualizar = [this] { if (!m_carregando) atualizarGrafico(); };
    connect(m_comboTurma, &QComboBox::currentIndexChanged, this, [this] {
        if (m_carregando)
            return;
        recarregarAlunos();
        atualizarGrafico();
    });
    connect(m_comboPeriodo, &QComboBox::currentIndexChanged, this, atualizar);
    connect(m_comboGrafico, &QComboBox::currentIndexChanged, this, atualizar);
    connect(m_comboAluno, &QComboBox::currentIndexChanged, this, atualizar);
    connect(btnPng, &QPushButton::clicked, this, &RelatoriosPage::salvarGraficoPng);
    connect(btnBoletim, &QPushButton::clicked, this, &RelatoriosPage::gerarBoletim);
    connect(btnFrequencia, &QPushButton::clicked, this, &RelatoriosPage::gerarFrequencia);
    connect(btnFicha, &QPushButton::clicked, this, &RelatoriosPage::gerarFicha);
}

RelatoriosPage::~RelatoriosPage()
{
    delete m_servico;
}

void RelatoriosPage::showEvent(QShowEvent *evento)
{
    QWidget::showEvent(evento);
    recarregarTurmas();
}

// ============================================================================

double RelatoriosPage::notaDeCorte() const { return QSettings().value(QStringLiteral("notaCorte"), 6.0).toDouble(); }
int RelatoriosPage::turmaAtualId() const { return m_comboTurma->currentData().toInt(); }
int RelatoriosPage::periodoAtual() const { return m_comboPeriodo->currentData().toInt(); }
int RelatoriosPage::alunoAtualId() const { return m_comboAluno->currentData().toInt(); }

void RelatoriosPage::recarregarTurmas()
{
    m_carregando = true;
    const int anterior = turmaAtualId();
    m_comboTurma->clear();
    for (const Turma &t : m_turmas.listar(false))
        m_comboTurma->addItem(t.disciplina.isEmpty() ? t.nome : QStringLiteral("%1 — %2").arg(t.nome, t.disciplina), t.id);
    const int idx = m_comboTurma->findData(anterior);
    m_comboTurma->setCurrentIndex(idx >= 0 ? idx : (m_comboTurma->count() > 0 ? 0 : -1));
    recarregarAlunos();
    m_carregando = false;
    atualizarGrafico();
}

void RelatoriosPage::recarregarAlunos()
{
    const int anterior = alunoAtualId();
    const bool antes = m_carregando;
    m_carregando = true;
    m_comboAluno->clear();
    for (const Aluno &a : m_alunos.listarPorTurma(turmaAtualId(), QString(), false))
        m_comboAluno->addItem(a.nome, a.id);
    const int idx = m_comboAluno->findData(anterior);
    m_comboAluno->setCurrentIndex(idx >= 0 ? idx : (m_comboAluno->count() > 0 ? 0 : -1));
    m_carregando = antes;
}

void RelatoriosPage::atualizarGrafico()
{
    const auto tipo = static_cast<Grafico>(m_comboGrafico->currentData().toInt());
    m_comboAluno->setEnabled(tipo == EvolucaoDoAluno);

    const auto boletim = m_servico->boletim(turmaAtualId(), periodoAtual());
    if (!boletim) {
        m_grafico->setDados({}, ChartWidget::Tipo::Barras, QStringLiteral("Relatórios"), 10.0);
        m_resumo->setText(QStringLiteral("Cadastre uma turma e seus alunos para ver os gráficos."));
        return;
    }

    const double corte = notaDeCorte();
    QList<ChartWidget::Ponto> pontos;
    QString resumo;

    switch (tipo) {
    case MediaPorAluno:
        for (const LinhaBoletim &l : boletim->linhas)
            pontos.append({l.nome, l.media.value_or(0.0), l.media.has_value()});
        m_grafico->setDados(pontos, ChartWidget::Tipo::Barras, QStringLiteral("Média por aluno — %1").arg(boletim->turma.nome),
                            10.0, corte, QString(), 1);
        break;

    case MediaPorAvaliacao:
        for (int i = 0; i < boletim->avaliacoes.size(); ++i)
            pontos.append({boletim->avaliacoes.at(i).nome, boletim->mediaPorAvaliacao.at(i).value_or(0.0),
                           boletim->mediaPorAvaliacao.at(i).has_value()});
        m_grafico->setDados(pontos, ChartWidget::Tipo::Barras,
                            QStringLiteral("Média da turma por avaliação (escala 0–10) — %1").arg(boletim->turma.nome), 10.0, corte,
                            QString(), 1);
        break;

    case DistribuicaoDeMedias: {
        const QVector<int> faixas = DesempenhoService::distribuicaoDeMedias(*boletim, 5, 10.0);
        int maior = 1;
        for (int v : faixas)
            maior = std::max(maior, v);
        for (int i = 0; i < faixas.size(); ++i)
            pontos.append({QStringLiteral("%1 a %2").arg(i * 2).arg((i + 1) * 2), double(faixas.at(i)), true});
        // Topo do eixo em múltiplo de 5 para o eixo Y ter degraus inteiros.
        const double topo = std::ceil(maior / 5.0) * 5.0;
        m_grafico->setDados(pontos, ChartWidget::Tipo::Barras,
                            QStringLiteral("Quantos alunos em cada faixa de média — %1").arg(boletim->turma.nome), topo, -1.0,
                            QString(), 0);
        break;
    }

    case FrequenciaPorAluno:
        for (const LinhaBoletim &l : boletim->linhas)
            pontos.append({l.nome, l.frequenciaPct.value_or(0.0), l.frequenciaPct.has_value()});
        m_grafico->setDados(pontos, ChartWidget::Tipo::Barras, QStringLiteral("Frequência por aluno — %1").arg(boletim->turma.nome),
                            100.0, FrequenciaUtil::kFrequenciaMinima, QStringLiteral("%"), 0);
        break;

    case EvolucaoDoAluno: {
        const auto ficha = m_servico->ficha(alunoAtualId());
        if (!ficha) {
            m_grafico->setDados({}, ChartWidget::Tipo::Linha, QStringLiteral("Evolução do aluno"), 10.0);
            m_resumo->setText(QStringLiteral("Escolha um aluno."));
            return;
        }
        for (int i = 0; i < ficha->avaliacoes.size(); ++i) {
            const Avaliacao &a = ficha->avaliacoes.at(i);
            const auto &nota = ficha->notas.at(i);
            // Normaliza para 0-10 (uma prova de 5 pontos e uma de 10 ficam comparáveis).
            pontos.append({a.nome, nota && a.notaMaxima > 0 ? *nota / a.notaMaxima * 10.0 : 0.0, nota.has_value()});
        }
        m_grafico->setDados(pontos, ChartWidget::Tipo::Linha,
                            QStringLiteral("Evolução de %1 (escala 0–10)").arg(ficha->aluno.nome), 10.0, corte, QString(), 1);
        break;
    }
    }

    // Resumo em texto abaixo do gráfico
    int comMedia = 0, abaixo = 0;
    for (const LinhaBoletim &l : boletim->linhas) {
        if (l.media) {
            ++comMedia;
            if (*l.media < corte)
                ++abaixo;
        }
    }
    if (boletim->mediaTurma)
        resumo = QStringLiteral("Média da turma: %1 · %2 aluno(s) com média · %3 abaixo da nota de corte (%4)")
                     .arg(numero(*boletim->mediaTurma)).arg(comMedia).arg(abaixo).arg(numero(corte, 1));
    else
        resumo = QStringLiteral("Ainda não há notas lançadas nesta seleção.");
    m_resumo->setText(resumo);
}

// ============================================================================
// Exportação
// ============================================================================

void RelatoriosPage::salvarGraficoPng()
{
    const QString pasta = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString caminho = QFileDialog::getSaveFileName(this, QStringLiteral("Salvar gráfico"),
                                                   pasta + QStringLiteral("/grafico.png"),
                                                   QStringLiteral("Imagem PNG (*.png)"));
    if (caminho.isEmpty())
        return;
    if (!caminho.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive))
        caminho += QStringLiteral(".png");
    if (!m_grafico->grab().save(caminho))
        QMessageBox::critical(this, QStringLiteral("Erro"), QStringLiteral("Não foi possível salvar a imagem."));
}

void RelatoriosPage::salvarEAbrirPdf(const QString &html, const QString &nomeSugerido, const QString &titulo, bool paisagem)
{
    const QString pasta = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString caminho = QFileDialog::getSaveFileName(this, QStringLiteral("Salvar relatório em PDF"),
                                                   pasta + QLatin1Char('/') + nomeDeArquivoSeguro(nomeSugerido) + QStringLiteral(".pdf"),
                                                   QStringLiteral("Documento PDF (*.pdf)"));
    if (caminho.isEmpty())
        return;
    if (!caminho.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
        caminho += QStringLiteral(".pdf");

    QString erro;
    if (!RelatorioPdf::salvarPdf(html, caminho, titulo, paisagem, &erro)) {
        QMessageBox::critical(this, QStringLiteral("Erro ao gerar o PDF"), erro);
        return;
    }
    const auto resp = QMessageBox::question(this, QStringLiteral("Relatório gerado"),
                                            QStringLiteral("PDF salvo em:\n%1\n\nAbrir agora?").arg(caminho),
                                            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (resp == QMessageBox::Yes)
        QDesktopServices::openUrl(QUrl::fromLocalFile(caminho));
}

void RelatoriosPage::gerarBoletim()
{
    const auto boletim = m_servico->boletim(turmaAtualId(), periodoAtual());
    if (!boletim) {
        QMessageBox::information(this, QStringLiteral("Sem turma"), QStringLiteral("Escolha uma turma."));
        return;
    }
    // Muitas colunas de avaliação pedem página deitada.
    salvarEAbrirPdf(RelatorioPdf::htmlBoletim(*boletim, notaDeCorte()),
                    QStringLiteral("Boletim - %1").arg(boletim->turma.nome),
                    QStringLiteral("Boletim — %1").arg(boletim->turma.nome), boletim->avaliacoes.size() > 6);
}

void RelatoriosPage::gerarFrequencia()
{
    const auto boletim = m_servico->boletim(turmaAtualId(), 0);
    if (!boletim) {
        QMessageBox::information(this, QStringLiteral("Sem turma"), QStringLiteral("Escolha uma turma."));
        return;
    }
    salvarEAbrirPdf(RelatorioPdf::htmlFrequencia(*boletim),
                    QStringLiteral("Frequência - %1").arg(boletim->turma.nome),
                    QStringLiteral("Frequência — %1").arg(boletim->turma.nome), false);
}

void RelatoriosPage::gerarFicha()
{
    const auto ficha = m_servico->ficha(alunoAtualId());
    if (!ficha) {
        QMessageBox::information(this, QStringLiteral("Sem aluno"), QStringLiteral("Escolha um aluno na lista."));
        return;
    }
    salvarEAbrirPdf(RelatorioPdf::htmlFicha(*ficha, notaDeCorte()),
                    QStringLiteral("Ficha - %1").arg(ficha->aluno.nome),
                    QStringLiteral("Ficha do aluno — %1").arg(ficha->aluno.nome), false);
}
