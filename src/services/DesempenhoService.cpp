#include "services/DesempenhoService.h"

#include "core/FrequenciaUtil.h"
#include "core/MediaCalculator.h"
#include "database/AlunoRepository.h"
#include "database/AvaliacaoRepository.h"
#include "database/FrequenciaRepository.h"
#include "database/NotaRepository.h"
#include "database/Repositorios.h"
#include "database/TurmaRepository.h"

#include <algorithm>
#include <vector>

namespace {

// Média ponderada das notas lançadas de uma lista de avaliações.
std::optional<double> mediaDasNotas(const QList<Avaliacao> &avaliacoes, const QHash<qint64, double> &notas,
                                    int alunoId)
{
    std::vector<MediaCalculator::Item> itens;
    for (const Avaliacao &a : avaliacoes) {
        const auto it = notas.constFind(NotaRepository::chave(a.id, alunoId));
        if (it != notas.constEnd())
            itens.push_back({it.value(), a.peso, a.notaMaxima});
    }
    return MediaCalculator::ponderada(itens);
}

std::optional<double> percentualDe(const ResumoFrequencia &f)
{
    return FrequenciaUtil::percentual(f.presencas, f.atrasos, f.faltas, f.justificadas);
}

}  // namespace

std::optional<Boletim> DesempenhoService::boletim(int turmaId, int periodo)
{
    const auto turma = m_repos.turmas.buscar(turmaId);
    if (!turma)
        return std::nullopt;

    Boletim b;
    b.turma = *turma;
    b.periodo = periodo;
    b.avaliacoes = m_repos.avaliacoes.listarPorTurma(turmaId, periodo);

    const QHash<qint64, double> notas = m_repos.notas.listarPorTurma(turmaId);
    // A frequência considera todas as chamadas da turma (não há período nas chamadas).
    const QHash<int, ResumoFrequencia> frequencias = m_repos.frequencia.resumoPorAluno(turmaId);

    double somaMedias = 0.0;
    int comMedia = 0;

    for (const Aluno &aluno : m_repos.alunos.listarPorTurma(turmaId, QString(), /*incluirInativos=*/false)) {
        LinhaBoletim l;
        l.alunoId = aluno.id;
        l.nome = aluno.nome;
        l.matricula = aluno.matricula;
        for (const Avaliacao &a : b.avaliacoes) {
            const auto it = notas.constFind(NotaRepository::chave(a.id, aluno.id));
            l.notas.append(it != notas.constEnd() ? std::optional<double>(it.value()) : std::nullopt);
        }
        l.media = mediaDasNotas(b.avaliacoes, notas, aluno.id);
        l.frequencia = frequencias.value(aluno.id);
        l.frequenciaPct = percentualDe(l.frequencia);

        if (l.media) {
            somaMedias += *l.media;
            ++comMedia;
        }
        b.linhas.append(l);
    }
    if (comMedia > 0)
        b.mediaTurma = somaMedias / comMedia;

    // Média da turma em cada avaliação, normalizada para 0-10.
    for (int i = 0; i < b.avaliacoes.size(); ++i) {
        double soma = 0.0;
        int n = 0;
        for (const LinhaBoletim &l : b.linhas) {
            if (l.notas.at(i) && b.avaliacoes.at(i).notaMaxima > 0) {
                soma += *l.notas.at(i) / b.avaliacoes.at(i).notaMaxima * 10.0;
                ++n;
            }
        }
        b.mediaPorAvaliacao.append(n > 0 ? std::optional<double>(soma / n) : std::nullopt);
    }
    return b;
}

std::optional<FichaAluno> DesempenhoService::ficha(int alunoId)
{
    const auto aluno = m_repos.alunos.buscar(alunoId);
    if (!aluno)
        return std::nullopt;
    const auto turma = m_repos.turmas.buscar(aluno->turmaId);
    if (!turma)
        return std::nullopt;

    FichaAluno f;
    f.aluno = *aluno;
    f.turma = *turma;
    f.avaliacoes = m_repos.avaliacoes.listarPorTurma(aluno->turmaId, 0);

    const QHash<qint64, double> notas = m_repos.notas.listarPorTurma(aluno->turmaId);
    for (const Avaliacao &a : f.avaliacoes) {
        const auto it = notas.constFind(NotaRepository::chave(a.id, alunoId));
        f.notas.append(it != notas.constEnd() ? std::optional<double>(it.value()) : std::nullopt);
    }
    f.media = mediaDasNotas(f.avaliacoes, notas, alunoId);

    f.frequencia = m_repos.frequencia.resumoPorAluno(aluno->turmaId).value(alunoId);
    f.frequenciaPct = percentualDe(f.frequencia);
    for (const RegistroFrequencia &r : m_repos.frequencia.doAluno(alunoId)) {
        if (r.situacao != QLatin1Char('P'))
            f.ocorrencias.append(r);
    }
    return f;
}

QVector<int> DesempenhoService::distribuicaoDeMedias(const Boletim &b, int faixas, double maximo)
{
    QVector<int> contagem(std::max(1, faixas), 0);
    const double largura = maximo / contagem.size();
    for (const LinhaBoletim &l : b.linhas) {
        if (!l.media)
            continue;
        // A última faixa inclui o valor máximo (ex.: 10,0 cai em [8, 10]).
        const int i = std::clamp(static_cast<int>(*l.media / largura), 0, static_cast<int>(contagem.size()) - 1);
        ++contagem[i];
    }
    return contagem;
}
