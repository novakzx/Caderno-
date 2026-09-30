#include "ui/NotasTableModel.h"

#include "core/MediaCalculator.h"
#include "database/AlunoRepository.h"
#include "database/AvaliacaoRepository.h"
#include "database/NotaRepository.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QLocale>
#include <cmath>

namespace {

// 8.5 -> "8,5" ; 10.0 -> "10" ; 7.25 -> "7,25" (vírgula decimal, sem zeros sobrando)
QString formatarNota(double v)
{
    QString s = QString::number(v, 'f', 2);
    while (s.endsWith(QLatin1Char('0')))
        s.chop(1);
    if (s.endsWith(QLatin1Char('.')))
        s.chop(1);
    return s.replace(QLatin1Char('.'), QLatin1Char(','));
}

QString formatarMedia(double v)
{
    return QString::number(v, 'f', 2).replace(QLatin1Char('.'), QLatin1Char(','));
}

// Aceita "8,5", "8.5" e espaços. Retorna false se não for um número finito.
bool lerNumero(const QString &texto, double *saida)
{
    QString t = texto.trimmed();
    t.remove(QLatin1Char(' '));
    t.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double v = t.toDouble(&ok);
    if (!ok || !std::isfinite(v))
        return false;
    *saida = v;
    return true;
}

}  // namespace

NotasTableModel::NotasTableModel(AvaliacaoRepository &avaliacoes, AlunoRepository &alunos,
                                 NotaRepository &notas, QObject *parent)
    : QAbstractTableModel(parent),
      m_repoAvaliacoes(avaliacoes),
      m_repoAlunos(alunos),
      m_repoNotas(notas)
{
}

void NotasTableModel::carregar(int turmaId, int periodo)
{
    beginResetModel();
    m_turmaId = turmaId;
    m_periodo = periodo;
    m_alunos.clear();
    m_avaliacoes.clear();
    m_notas.clear();

    if (turmaId != 0) {
        m_alunos = m_repoAlunos.listarPorTurma(turmaId, QString(), /*incluirInativos=*/false);
        m_avaliacoes = m_repoAvaliacoes.listarPorTurma(turmaId, periodo);
        m_notas = m_repoNotas.listarPorTurma(turmaId);
    }
    recalcularMedias();
    endResetModel();
}

void NotasTableModel::setNotaCorte(double corte)
{
    m_notaCorte = corte;
    if (!m_alunos.isEmpty())
        emit dataChanged(index(0, colunaMedia()), index(m_alunos.size() - 1, colunaMedia()),
                         {Qt::ForegroundRole});
}

const Avaliacao *NotasTableModel::avaliacaoDaColuna(int coluna) const
{
    const int i = coluna - 1;
    return (i >= 0 && i < m_avaliacoes.size()) ? &m_avaliacoes.at(i) : nullptr;
}

int NotasTableModel::colunaDaAvaliacao(int avaliacaoId) const
{
    for (int i = 0; i < m_avaliacoes.size(); ++i)
        if (m_avaliacoes.at(i).id == avaliacaoId)
            return i + 1;
    return -1;
}

int NotasTableModel::alunoIdDaLinha(int linha) const
{
    return (linha >= 0 && linha < m_alunos.size()) ? m_alunos.at(linha).id : 0;
}

// Média ponderada da linha: só entram as avaliações com nota lançada.
std::optional<double> NotasTableModel::mediaDaLinha(int linha) const
{
    std::vector<MediaCalculator::Item> itens;
    const int alunoId = m_alunos.at(linha).id;
    for (const Avaliacao &a : m_avaliacoes) {
        const auto it = m_notas.constFind(NotaRepository::chave(a.id, alunoId));
        if (it != m_notas.constEnd())
            itens.push_back({it.value(), a.peso, a.notaMaxima});
    }
    return MediaCalculator::ponderada(itens);
}

void NotasTableModel::recalcularMedias()
{
    m_medias.assign(m_alunos.size(), std::nullopt);
    for (int i = 0; i < m_alunos.size(); ++i)
        m_medias[i] = mediaDaLinha(i);
}

ResumoTurma NotasTableModel::resumo() const
{
    ResumoTurma r;
    double soma = 0.0;
    for (const auto &m : m_medias) {
        if (!m)
            continue;
        ++r.alunosComMedia;
        soma += *m;
        if (*m < m_notaCorte)
            ++r.abaixoDaCorte;
    }
    if (r.alunosComMedia > 0)
        r.mediaGeral = soma / r.alunosComMedia;
    return r;
}

int NotasTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_alunos.size();
}

int NotasTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_avaliacoes.size() + 2;  // aluno + avaliações + média
}

QVariant NotasTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_alunos.size())
        return {};

    const int linha = index.row();
    const int coluna = index.column();

    // Coluna do aluno
    if (coluna == 0) {
        if (role == Qt::DisplayRole || role == Qt::EditRole)
            return m_alunos.at(linha).nome;
        if (role == Qt::ToolTipRole && !m_alunos.at(linha).matricula.isEmpty())
            return QStringLiteral("Matrícula: %1").arg(m_alunos.at(linha).matricula);
        return {};
    }

    // Coluna da média
    if (coluna == colunaMedia()) {
        const auto &m = m_medias[linha];
        switch (role) {
        case Qt::DisplayRole:
            return m ? formatarMedia(*m) : QStringLiteral("—");
        case Qt::TextAlignmentRole:
            return int(Qt::AlignCenter);
        case Qt::FontRole: {
            QFont f;
            f.setBold(true);
            return f;
        }
        case Qt::ForegroundRole:
            // Média abaixo da nota de corte aparece em vermelho.
            if (m && *m < m_notaCorte)
                return QBrush(QColor(QStringLiteral("#D64545")));
            return {};
        case Qt::ToolTipRole:
            return m ? QStringLiteral("Média ponderada das notas lançadas")
                     : QStringLiteral("Ainda não há notas lançadas");
        default:
            return {};
        }
    }

    // Colunas de avaliação
    const Avaliacao *av = avaliacaoDaColuna(coluna);
    if (!av)
        return {};
    const auto it = m_notas.constFind(NotaRepository::chave(av->id, m_alunos.at(linha).id));
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return it != m_notas.constEnd() ? QVariant(formatarNota(it.value())) : QVariant(QString());
    case Qt::TextAlignmentRole:
        return int(Qt::AlignCenter);
    default:
        return {};
    }
}

bool NotasTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || !index.isValid() || index.row() >= m_alunos.size())
        return false;
    const Avaliacao *av = avaliacaoDaColuna(index.column());
    if (!av)
        return false;

    const QString texto = value.toString().trimmed();
    std::optional<double> novo;  // nullopt = apagar a nota
    if (!texto.isEmpty()) {
        double v = 0.0;
        if (!lerNumero(texto, &v)) {
            emit mensagemDeErro(QStringLiteral("\"%1\" não é um número válido.").arg(texto));
            return false;
        }
        if (v < 0.0 || v > av->notaMaxima) {
            emit mensagemDeErro(QStringLiteral("A nota de \"%1\" deve estar entre 0 e %2.")
                                    .arg(av->nome, formatarNota(av->notaMaxima)));
            return false;
        }
        novo = v;
    }

    const int linha = index.row();
    const int alunoId = m_alunos.at(linha).id;
    if (!m_repoNotas.salvar(av->id, alunoId, novo)) {
        emit mensagemDeErro(QStringLiteral("Não foi possível gravar a nota: %1").arg(m_repoNotas.ultimoErro()));
        return false;
    }

    const qint64 chave = NotaRepository::chave(av->id, alunoId);
    if (novo)
        m_notas.insert(chave, *novo);
    else
        m_notas.remove(chave);

    m_medias[linha] = mediaDaLinha(linha);
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    const QModelIndex celulaMedia = this->index(linha, colunaMedia());
    emit dataChanged(celulaMedia, celulaMedia);
    return true;
}

Qt::ItemFlags NotasTableModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QAbstractTableModel::flags(index);
    if (avaliacaoDaColuna(index.column()))
        f |= Qt::ItemIsEditable;
    else
        f &= ~Qt::ItemIsEditable;
    return f;
}

QVariant NotasTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal) {
        if (role == Qt::DisplayRole)
            return section + 1;  // número da linha
        return {};
    }

    if (section == 0)
        return role == Qt::DisplayRole ? QVariant(QStringLiteral("Aluno")) : QVariant();
    if (section == colunaMedia())
        return role == Qt::DisplayRole ? QVariant(QStringLiteral("Média")) : QVariant();

    const Avaliacao *av = avaliacaoDaColuna(section);
    if (!av)
        return {};
    if (role == Qt::DisplayRole)
        return QStringLiteral("%1\npeso %2 · máx %3").arg(av->nome, formatarNota(av->peso), formatarNota(av->notaMaxima));
    if (role == Qt::ToolTipRole) {
        QString dica = QStringLiteral("%1 (%2)\nPeríodo %3").arg(av->nome, av->tipo).arg(av->periodo);
        if (av->data.isValid())
            dica += QStringLiteral("\nData: %1").arg(av->data.toString(QStringLiteral("dd/MM/yyyy")));
        return dica;
    }
    if (role == Qt::TextAlignmentRole)
        return int(Qt::AlignCenter);
    return {};
}
