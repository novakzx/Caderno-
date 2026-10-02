#include "ui/SorteioDialog.h"

#include "database/FrequenciaRepository.h"
#include "ui/ThemeManager.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDate>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <chrono>

namespace {

constexpr int kPassosDaAnimacao = 18;  // quantos nomes "passam" antes de parar

std::mt19937 novoGerador()
{
    std::random_device rd;  // sorteado de verdade; o tempo entra junto caso o sistema não ofereça entropia
    std::seed_seq semente{rd(), rd(), rd(), static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count())};
    return std::mt19937(semente);
}

}  // namespace

SorteioDialog::SorteioDialog(const QList<Aluno> &alunosAtivos, FrequenciaRepository &frequencia, int turmaId,
                             const QString &turmaNome, QWidget *parent)
    : QDialog(parent), m_alunos(alunosAtivos), m_frequencia(frequencia), m_turmaId(turmaId), m_rng(novoGerador())
{
    setWindowTitle(QStringLiteral("Sorteio — %1").arg(turmaNome));
    setMinimumSize(560, 520);

    auto *titulo = new QLabel(QStringLiteral("Sorteio e grupos — %1").arg(turmaNome));
    titulo->setObjectName(QStringLiteral("sectionTitle"));
    titulo->setTextFormat(Qt::PlainText);

    m_soPresentes = new QCheckBox(QStringLiteral("Só quem está presente hoje (usa a chamada de hoje)"));
    m_aviso = new QLabel;
    m_aviso->setObjectName(QStringLiteral("muted"));
    m_aviso->setWordWrap(true);

    m_abas = new QTabWidget;

    // --- Aba: sortear um aluno ---
    auto *abaSorteio = new QWidget;
    auto *ls = new QVBoxLayout(abaSorteio);
    ls->setContentsMargins(12, 16, 12, 12);
    ls->setSpacing(14);
    m_nomeSorteado = new QLabel(QStringLiteral("Quem será?"));
    m_nomeSorteado->setAlignment(Qt::AlignCenter);
    m_nomeSorteado->setObjectName(QStringLiteral("nomeSorteado"));
    m_nomeSorteado->setTextFormat(Qt::PlainText);  // nome vem do usuário
    m_nomeSorteado->setWordWrap(true);
    m_nomeSorteado->setMinimumHeight(120);
    m_semRepeticao = new QCheckBox(QStringLiteral("Não repetir até todos serem sorteados"));
    m_semRepeticao->setChecked(true);
    m_contagem = new QLabel;
    m_contagem->setObjectName(QStringLiteral("muted"));
    m_botaoSortear = new QPushButton(QStringLiteral("Sortear"));
    m_botaoSortear->setObjectName(QStringLiteral("primary"));
    ThemeManager::iconeNoBotao(m_botaoSortear, QStringLiteral("dado"), Tokens::Id::OnPrimary);
    ls->addStretch(1);
    ls->addWidget(m_nomeSorteado);
    ls->addWidget(m_contagem, 0, Qt::AlignHCenter);
    ls->addStretch(1);
    ls->addWidget(m_semRepeticao);
    ls->addWidget(m_botaoSortear);
    m_abas->addTab(abaSorteio, QStringLiteral("Sortear aluno"));

    // --- Aba: montar grupos ---
    auto *abaGrupos = new QWidget;
    auto *lg = new QVBoxLayout(abaGrupos);
    lg->setContentsMargins(12, 16, 12, 12);
    lg->setSpacing(10);
    m_porQuantidade = new QRadioButton(QStringLiteral("Quantidade de grupos"));
    m_porTamanho = new QRadioButton(QStringLiteral("Alunos por grupo"));
    m_porQuantidade->setChecked(true);
    m_numero = new QSpinBox;
    m_numero->setRange(1, 40);
    m_numero->setValue(4);
    auto *linha = new QHBoxLayout;
    linha->addWidget(m_porQuantidade);
    linha->addWidget(m_porTamanho);
    linha->addStretch(1);
    linha->addWidget(m_numero);
    auto *montar = new QPushButton(QStringLiteral("Montar grupos"));
    montar->setObjectName(QStringLiteral("primary"));
    ThemeManager::iconeNoBotao(montar, QStringLiteral("turmas"), Tokens::Id::OnPrimary);
    m_resultadoDosGrupos = new QPlainTextEdit;
    m_resultadoDosGrupos->setReadOnly(true);
    m_resultadoDosGrupos->setPlaceholderText(QStringLiteral("Os grupos aparecem aqui. Clique em \"Montar grupos\" de novo para refazer."));
    m_botaoCopiar = new QPushButton(QStringLiteral("Copiar"));
    ThemeManager::iconeNoBotao(m_botaoCopiar, QStringLiteral("copiar"));
    m_botaoCopiar->setEnabled(false);
    lg->addLayout(linha);
    lg->addWidget(montar);
    lg->addWidget(m_resultadoDosGrupos, 1);
    lg->addWidget(m_botaoCopiar, 0, Qt::AlignLeft);
    m_abas->addTab(abaGrupos, QStringLiteral("Montar grupos"));

    auto *fechar = new QDialogButtonBox(QDialogButtonBox::Close);
    fechar->button(QDialogButtonBox::Close)->setText(QStringLiteral("Fechar"));
    connect(fechar, &QDialogButtonBox::rejected, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(10);
    layout->addWidget(titulo);
    layout->addWidget(m_soPresentes);
    layout->addWidget(m_aviso);
    layout->addWidget(m_abas, 1);
    layout->addWidget(fechar);

    m_animacao = new QTimer(this);
    connect(m_animacao, &QTimer::timeout, this, &SorteioDialog::passoDaAnimacao);
    connect(m_botaoSortear, &QPushButton::clicked, this, &SorteioDialog::sortearAluno);
    connect(montar, &QPushButton::clicked, this, &SorteioDialog::montarGrupos);
    connect(m_botaoCopiar, &QPushButton::clicked, this, &SorteioDialog::copiarGrupos);
    connect(m_soPresentes, &QCheckBox::toggled, this, [this] {
        reiniciarFila();
        atualizarContagem();
    });
    connect(m_semRepeticao, &QCheckBox::toggled, this, [this] { reiniciarFila(); atualizarContagem(); });

    reiniciarFila();
    atualizarContagem();
}

QList<Aluno> SorteioDialog::participantes() const
{
    if (!m_soPresentes->isChecked())
        return m_alunos;
    // Ausente = falta (F) ou falta justificada (J) na chamada de hoje. Sem chamada de hoje, todos contam.
    const auto chamada = m_frequencia.doDia(m_turmaId, QDate::currentDate());
    QList<Aluno> presentes;
    for (const Aluno &a : m_alunos) {
        const auto it = chamada.constFind(a.id);
        const bool ausente = it != chamada.constEnd() && (it->situacao == QLatin1Char('F') || it->situacao == QLatin1Char('J'));
        if (!ausente)
            presentes.append(a);
    }
    return presentes;
}

void SorteioDialog::reiniciarFila()
{
    m_filaDeAlunos = participantes();
    m_fila = std::make_unique<SorteioUtil::SorteioSemRepeticao>(static_cast<int>(m_filaDeAlunos.size()));
    m_nomeSorteado->setText(QStringLiteral("Quem será?"));
}

void SorteioDialog::atualizarContagem()
{
    const int total = static_cast<int>(m_filaDeAlunos.size());
    if (m_alunos.isEmpty())
        m_aviso->setText(QStringLiteral("Esta turma não tem alunos ativos."));
    else if (m_soPresentes->isChecked())
        m_aviso->setText(QStringLiteral("%1 de %2 alunos presentes entram no sorteio.").arg(total).arg(m_alunos.size()));
    else
        m_aviso->setText(QStringLiteral("%1 alunos entram no sorteio.").arg(total));
    m_contagem->setText(m_semRepeticao->isChecked() ? QStringLiteral("Faltam sortear: %1 de %2").arg(m_fila->restantes()).arg(total) : QString());
    m_botaoSortear->setEnabled(total > 0 && !m_animacao->isActive());
}

void SorteioDialog::sortearAluno()
{
    if (m_filaDeAlunos.isEmpty())
        return;
    bool recomecou = false;
    if (m_semRepeticao->isChecked()) {
        m_escolhido = m_fila->proximo(m_rng, &recomecou);
    } else {
        std::uniform_int_distribution<int> d(0, static_cast<int>(m_filaDeAlunos.size()) - 1);
        m_escolhido = d(m_rng);
    }
    if (recomecou)
        m_contagem->setText(QStringLiteral("Todos já saíram: nova rodada!"));
    m_passos = 0;
    m_totalDePassos = kPassosDaAnimacao;
    m_botaoSortear->setEnabled(false);
    m_animacao->start(45);
}

// Animação: os nomes passam rápido e vão desacelerando até parar no sorteado.
void SorteioDialog::passoDaAnimacao()
{
    ++m_passos;
    if (m_passos >= m_totalDePassos) {
        m_animacao->stop();
        m_nomeSorteado->setText(m_filaDeAlunos.at(m_escolhido).nome);
        atualizarContagem();
        return;
    }
    std::uniform_int_distribution<int> d(0, static_cast<int>(m_filaDeAlunos.size()) - 1);
    m_nomeSorteado->setText(m_filaDeAlunos.at(d(m_rng)).nome);
    m_animacao->setInterval(40 + (m_passos * m_passos) / 2);  // desacelera
}

void SorteioDialog::montarGrupos()
{
    const QList<Aluno> lista = participantes();
    const int n = static_cast<int>(lista.size());
    if (n == 0) {
        m_resultadoDosGrupos->setPlainText(QStringLiteral("Não há alunos para dividir."));
        m_botaoCopiar->setEnabled(false);
        return;
    }
    const int grupos = m_porQuantidade->isChecked() ? m_numero->value() : SorteioUtil::gruposParaTamanho(n, m_numero->value());
    const auto divisao = SorteioUtil::dividirEmGrupos(n, grupos, m_rng);

    QStringList blocos;
    for (std::size_t g = 0; g < divisao.size(); ++g) {
        QStringList nomes;
        for (int indice : divisao[g])
            nomes << lista.at(indice).nome;
        nomes.sort(Qt::CaseInsensitive);
        blocos << QStringLiteral("Grupo %1 (%2):\n  %3").arg(g + 1).arg(nomes.size()).arg(nomes.join(QStringLiteral("\n  ")));
    }
    m_resultadoDosGrupos->setPlainText(blocos.join(QStringLiteral("\n\n")));
    m_botaoCopiar->setEnabled(true);
}

void SorteioDialog::copiarGrupos()
{
    QApplication::clipboard()->setText(m_resultadoDosGrupos->toPlainText());
}
