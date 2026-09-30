#include "ui/BuscaDialog.h"
#include "ui/ThemeManager.h"

#include "core/TextoUtil.h"

#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace {

constexpr int kMaximoDeResultados = 40;

// Ícone de cada tipo de resultado (resources/icons).
QIcon iconeDoTipo(TipoBusca tipo)
{
    const char *nome = "pin";
    switch (tipo) {
    case TipoBusca::Turma:    nome = "turmas"; break;
    case TipoBusca::Aluno:    nome = "aluno"; break;
    case TipoBusca::Anotacao: nome = "anotacoes"; break;
    case TipoBusca::Aula:     nome = "aulas"; break;
    case TipoBusca::Tarefa:   nome = "tarefa-ok"; break;
    case TipoBusca::Evento:   nome = "calendario"; break;
    case TipoBusca::Anexo:    nome = "anexo"; break;
    }
    return ThemeManager::iconeColorido(QLatin1String(nome), Tokens::Id::InkMuted, 20);
}

// Em empates, turmas e alunos vêm antes dos demais.
int prioridadeDoTipo(TipoBusca tipo)
{
    switch (tipo) {
    case TipoBusca::Turma:    return 0;
    case TipoBusca::Aluno:    return 1;
    case TipoBusca::Aula:     return 2;
    case TipoBusca::Anotacao: return 3;
    case TipoBusca::Tarefa:   return 4;
    case TipoBusca::Evento:   return 5;
    case TipoBusca::Anexo:    return 6;
    }
    return 9;
}

}  // namespace

BuscaDialog::BuscaDialog(BuscaRepository &busca, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Busca global"));
    resize(640, 480);

    m_indice = busca.carregarIndice();  // carregado uma vez; a filtragem é em memória

    m_campo = new QLineEdit;
    m_campo->setPlaceholderText(QStringLiteral("Buscar turmas, alunos, anotações, aulas, tarefas, eventos, arquivos…"));
    m_campo->setClearButtonEnabled(true);
    m_campo->setObjectName(QStringLiteral("campoBusca"));  // tamanho vem do QSS (ThemeManager)
    m_campo->installEventFilter(this);  // para as setas ↑/↓ moverem a seleção

    m_lista = new QListWidget;
    m_lista->setAlternatingRowColors(true);

    m_dica = new QLabel;
    m_dica->setObjectName(QStringLiteral("muted"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 14);
    layout->setSpacing(10);
    layout->addWidget(m_campo);
    layout->addWidget(m_lista, 1);
    layout->addWidget(m_dica);

    connect(m_campo, &QLineEdit::textChanged, this, &BuscaDialog::filtrar);
    connect(m_campo, &QLineEdit::returnPressed, this, &BuscaDialog::confirmar);
    connect(m_lista, &QListWidget::itemActivated, this, [this] { confirmar(); });

    filtrar();
    m_campo->setFocus();
}

void BuscaDialog::filtrar()
{
    m_lista->clear();
    m_resultados.clear();

    const QStringList termos = normalizarTexto(m_campo->text()).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (termos.isEmpty()) {
        m_dica->setText(QStringLiteral("%1 itens pesquisáveis · Enter abre o resultado · Esc fecha").arg(m_indice.size()));
        return;
    }

    struct Acerto { const ItemBusca *item; int pontos; };
    QList<Acerto> acertos;
    for (const ItemBusca &item : m_indice) {
        bool todos = true;
        for (const QString &termo : termos) {
            if (!item.textoNormalizado.contains(termo)) {
                todos = false;
                break;
            }
        }
        if (!todos)
            continue;

        // Pontuação: acerto no começo do título vale mais que no meio do texto.
        const QString titulo = normalizarTexto(item.titulo);
        int pontos = 0;
        for (const QString &termo : termos) {
            if (titulo.startsWith(termo))
                pontos += 3;
            else if (titulo.contains(termo))
                pontos += 2;
            else
                pontos += 1;
        }
        acertos.append({&item, pontos});
    }

    std::stable_sort(acertos.begin(), acertos.end(), [](const Acerto &a, const Acerto &b) {
        if (a.pontos != b.pontos)
            return a.pontos > b.pontos;
        return prioridadeDoTipo(a.item->tipo) < prioridadeDoTipo(b.item->tipo);
    });

    const int total = acertos.size();
    for (int i = 0; i < std::min<int>(total, kMaximoDeResultados); ++i) {
        const ItemBusca &item = *acertos.at(i).item;
        m_resultados.append(item);
        m_lista->addItem(new QListWidgetItem(iconeDoTipo(item.tipo),
                                             QStringLiteral("%1\n%2").arg(item.titulo, item.subtitulo)));
    }
    if (!m_resultados.isEmpty())
        m_lista->setCurrentRow(0);

    m_dica->setText(total == 0 ? QStringLiteral("Nenhum resultado.")
                               : total > kMaximoDeResultados
                                     ? QStringLiteral("Mostrando %1 de %2 resultados — refine a busca.").arg(kMaximoDeResultados).arg(total)
                                     : QStringLiteral("%1 resultado(s)").arg(total));
}

void BuscaDialog::confirmar()
{
    const int linha = m_lista->currentRow();
    if (linha < 0 || linha >= m_resultados.size())
        return;
    m_escolhido = m_resultados.at(linha);
    accept();
}

bool BuscaDialog::eventFilter(QObject *alvo, QEvent *evento)
{
    // ↑/↓ no campo de texto movem a seleção da lista sem tirar o foco do campo.
    if (alvo == m_campo && evento->type() == QEvent::KeyPress) {
        const int tecla = static_cast<QKeyEvent *>(evento)->key();
        if ((tecla == Qt::Key_Down || tecla == Qt::Key_Up) && m_lista->count() > 0) {
            const int proxima = qBound(0, m_lista->currentRow() + (tecla == Qt::Key_Down ? 1 : -1), m_lista->count() - 1);
            m_lista->setCurrentRow(proxima);
            return true;
        }
    }
    return QDialog::eventFilter(alvo, evento);
}
