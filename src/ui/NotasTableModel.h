#pragma once

#include "models/Aluno.h"
#include "models/Avaliacao.h"

#include <QAbstractTableModel>
#include <QHash>
#include <optional>
#include <vector>

class AlunoRepository;
class AvaliacaoRepository;
class NotaRepository;

struct ResumoTurma {
    int alunosComMedia = 0;
    int abaixoDaCorte = 0;
    std::optional<double> mediaGeral;
};

// Modelo da planilha de notas:
//   linhas  = alunos ativos da turma
//   colunas = [Aluno] [avaliação 1] ... [avaliação N] [Média]
//
// Editar uma célula grava a nota no banco na hora (via NotaRepository) e
// recalcula a média da linha. A média é sempre calculada, nunca armazenada.
class NotasTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    NotasTableModel(AvaliacaoRepository &avaliacoes, AlunoRepository &alunos,
                    NotaRepository &notas, QObject *parent = nullptr);

    // Recarrega tudo do banco. periodo = 0 mostra todos os períodos.
    void carregar(int turmaId, int periodo);
    void setNotaCorte(double corte);

    int turmaId() const { return m_turmaId; }
    int totalAlunos() const { return m_alunos.size(); }
    int totalAvaliacoes() const { return m_avaliacoes.size(); }

    // Colunas: 0 = aluno; 1..N = avaliações; N+1 = média.
    int colunaMedia() const { return m_avaliacoes.size() + 1; }
    const Avaliacao *avaliacaoDaColuna(int coluna) const;  // nullptr se não for avaliação
    int colunaDaAvaliacao(int avaliacaoId) const;           // -1 se não existe
    int alunoIdDaLinha(int linha) const;

    ResumoTurma resumo() const;

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

signals:
    // Emitido quando uma entrada é rejeitada (valor inválido ou erro ao gravar).
    void mensagemDeErro(const QString &mensagem);

private:
    std::optional<double> mediaDaLinha(int linha) const;
    void recalcularMedias();

    AvaliacaoRepository &m_repoAvaliacoes;
    AlunoRepository &m_repoAlunos;
    NotaRepository &m_repoNotas;

    int m_turmaId = 0;
    int m_periodo = 0;
    double m_notaCorte = 6.0;

    QList<Aluno> m_alunos;
    QList<Avaliacao> m_avaliacoes;
    QHash<qint64, double> m_notas;              // (avaliacaoId, alunoId) -> valor
    std::vector<std::optional<double>> m_medias;  // uma por linha (cache)
};
