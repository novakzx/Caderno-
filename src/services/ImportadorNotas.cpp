#include "services/ImportadorNotas.h"

#include "core/TextoUtil.h"
#include "database/AlunoRepository.h"
#include "database/AvaliacaoRepository.h"
#include "database/NotaRepository.h"

#include <QHash>
#include <QSqlDatabase>

PlanoImportacao ImportadorNotas::planejar(int turmaId, const XlsxService::Planilha &planilha)
{
    PlanoImportacao plano;

    // --- Colunas: casa por nome com avaliações existentes da turma ---
    QHash<QString, int> avaliacaoPorNome;
    for (const Avaliacao &a : m_avaliacoes.listarPorTurma(turmaId, 0)) {
        const QString chave = normalizarTexto(a.nome);
        if (!avaliacaoPorNome.contains(chave))
            avaliacaoPorNome.insert(chave, a.id);
    }

    for (const XlsxService::ColunaPlanilha &c : planilha.colunas) {
        PlanoImportacao::Coluna col;
        col.nome = c.nome;
        col.avaliacaoExistenteId = avaliacaoPorNome.value(normalizarTexto(c.nome), 0);
        col.peso = c.peso;
        col.notaMaxima = c.notaMaxima;
        col.periodo = c.periodo;
        plano.colunas.append(col);
    }

    // --- Linhas: casa cada linha com um aluno da turma ---
    QHash<QString, int> alunoPorMatricula, alunoPorNome;
    for (const Aluno &a : m_alunos.listarPorTurma(turmaId, QString(), true)) {
        if (!a.matricula.isEmpty())
            alunoPorMatricula.insert(normalizarTexto(a.matricula), a.id);
        alunoPorNome.insert(normalizarTexto(a.nome), a.id);
    }

    for (const XlsxService::LinhaPlanilha &l : planilha.linhas) {
        int id = 0;
        if (!l.matricula.isEmpty())
            id = alunoPorMatricula.value(normalizarTexto(l.matricula), 0);
        if (id == 0 && !l.nome.isEmpty())
            id = alunoPorNome.value(normalizarTexto(l.nome), 0);

        plano.alunoIdPorLinha.append(id);
        if (id != 0)
            ++plano.alunosEncontrados;
        else
            plano.alunosNaoEncontrados.append(l.nome.isEmpty() ? l.matricula : l.nome);
    }
    return plano;
}

bool ImportadorNotas::executar(int turmaId, const XlsxService::Planilha &planilha,
                               const PlanoImportacao &plano, ResultadoImportacao *resultado,
                               QString *erro)
{
    ResultadoImportacao r;
    QSqlDatabase db = QSqlDatabase::database();

    auto desfazer = [&](const QString &mensagem) {
        db.rollback();
        if (erro)
            *erro = mensagem;
        return false;
    };

    if (!db.transaction()) {
        if (erro)
            *erro = QStringLiteral("Não foi possível iniciar a gravação no banco de dados.");
        return false;
    }

    for (int i = 0; i < plano.colunas.size(); ++i) {
        const PlanoImportacao::Coluna &col = plano.colunas.at(i);
        if (!col.importar)
            continue;

        // Descobre (ou cria) a avaliação de destino e sua nota máxima.
        int avaliacaoId = col.avaliacaoExistenteId;
        double notaMaxima = col.notaMaxima;
        if (avaliacaoId == 0) {
            Avaliacao nova;
            nova.turmaId = turmaId;
            nova.nome = col.nome;
            nova.peso = col.peso;
            nova.notaMaxima = col.notaMaxima;
            nova.periodo = col.periodo;
            avaliacaoId = m_avaliacoes.inserir(nova);
            if (avaliacaoId == 0)
                return desfazer(m_avaliacoes.ultimoErro());
            ++r.avaliacoesCriadas;
        } else {
            const auto existente = m_avaliacoes.buscar(avaliacaoId);
            if (!existente)
                return desfazer(QStringLiteral("Avaliação de destino não encontrada."));
            notaMaxima = existente->notaMaxima;
        }

        // Grava as notas da coluna.
        for (int j = 0; j < planilha.linhas.size(); ++j) {
            const int alunoId = plano.alunoIdPorLinha.value(j, 0);
            const auto &notas = planilha.linhas.at(j).notas;
            if (alunoId == 0 || i >= notas.size() || !notas.at(i))
                continue;  // aluno desconhecido ou célula vazia: não mexe

            const double valor = *notas.at(i);
            if (valor < 0.0 || valor > notaMaxima) {
                ++r.notasIgnoradas;
                continue;
            }
            if (!m_notas.salvar(avaliacaoId, alunoId, valor))
                return desfazer(m_notas.ultimoErro());
            ++r.notasGravadas;
        }
    }

    if (!db.commit())
        return desfazer(QStringLiteral("Não foi possível concluir a gravação no banco de dados."));

    if (resultado)
        *resultado = r;
    return true;
}
