#include "database/BuscaRepository.h"

#include "core/TextoUtil.h"
#include "database/SqlUtil.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {

// Junta vários textos numa única string normalizada para a busca.
QString indexar(const QStringList &partes)
{
    return normalizarTexto(partes.join(QLatin1Char(' ')));
}

}  // namespace

QList<ItemBusca> BuscaRepository::carregarIndice()
{
    QList<ItemBusca> itens;
    QSqlQuery q;

    // --- Turmas ---
    if (q.exec(QStringLiteral("SELECT id, nome, disciplina, ano_letivo, sala FROM turmas WHERE arquivada = 0"))) {
        while (q.next()) {
            ItemBusca i;
            i.tipo = TipoBusca::Turma;
            i.id = i.turmaId = q.value(0).toInt();
            i.titulo = q.value(1).toString();
            i.subtitulo = QStringLiteral("Turma · %1 · %2").arg(q.value(2).toString(), q.value(3).toString());
            i.textoNormalizado = indexar({i.titulo, q.value(2).toString(), q.value(3).toString(), q.value(4).toString()});
            itens.append(i);
        }
    } else {
        m_erro = q.lastError().text();
    }

    // --- Alunos ---
    if (q.exec(QStringLiteral("SELECT a.id, a.turma_id, a.nome, a.matricula, a.email, a.observacoes, t.nome "
                              "FROM alunos a JOIN turmas t ON t.id = a.turma_id WHERE t.arquivada = 0"))) {
        while (q.next()) {
            ItemBusca i;
            i.tipo = TipoBusca::Aluno;
            i.id = q.value(0).toInt();
            i.turmaId = q.value(1).toInt();
            i.titulo = q.value(2).toString();
            i.subtitulo = QStringLiteral("Aluno · %1").arg(q.value(6).toString());
            i.textoNormalizado = indexar({i.titulo, q.value(3).toString(), q.value(4).toString(), q.value(5).toString()});
            itens.append(i);
        }
    }

    // --- Anotações (título, texto e tags) ---
    if (q.exec(QStringLiteral(
            "SELECT a.id, COALESCE(a.turma_id, 0), a.titulo, a.conteudo_texto, COALESCE(t.nome, ''), a.atualizada_em, "
            "COALESCE((SELECT group_concat(g.nome, ' ') FROM anotacao_tags x JOIN tags g ON g.id = x.tag_id "
            "          WHERE x.anotacao_id = a.id), '') "
            "FROM anotacoes a LEFT JOIN turmas t ON t.id = a.turma_id"))) {
        while (q.next()) {
            ItemBusca i;
            i.tipo = TipoBusca::Anotacao;
            i.id = q.value(0).toInt();
            i.turmaId = q.value(1).toInt();
            i.titulo = q.value(2).toString().isEmpty() ? QStringLiteral("(sem título)") : q.value(2).toString();
            QString contexto = QStringLiteral("Anotação");
            if (!q.value(4).toString().isEmpty())
                contexto += QStringLiteral(" · ") + q.value(4).toString();
            if (!q.value(6).toString().isEmpty())
                contexto += QStringLiteral(" · #") + q.value(6).toString().replace(QLatin1Char(' '), QStringLiteral(" #"));
            i.subtitulo = contexto;
            i.data = lerData(q.value(5));
            i.textoNormalizado = indexar({q.value(2).toString(), q.value(3).toString(), q.value(6).toString()});
            itens.append(i);
        }
    }

    // --- Aulas (planos de aula) ---
    if (q.exec(QStringLiteral(
            "SELECT a.id, a.turma_id, a.data, a.tema, a.objetivos, a.materiais, a.observacoes, t.nome "
            "FROM aulas a JOIN turmas t ON t.id = a.turma_id WHERE t.arquivada = 0"))) {
        while (q.next()) {
            ItemBusca i;
            i.tipo = TipoBusca::Aula;
            i.id = q.value(0).toInt();
            i.turmaId = q.value(1).toInt();
            i.data = lerData(q.value(2));
            i.titulo = q.value(3).toString().isEmpty() ? QStringLiteral("(aula sem tema)") : q.value(3).toString();
            i.subtitulo = QStringLiteral("Aula · %1 · %2").arg(q.value(7).toString(), i.data.toString(QStringLiteral("dd/MM/yyyy")));
            i.textoNormalizado = indexar({q.value(3).toString(), q.value(4).toString(), q.value(5).toString(), q.value(6).toString()});
            itens.append(i);
        }
    }

    // --- Tarefas ---
    if (q.exec(QStringLiteral("SELECT t.id, COALESCE(t.turma_id, 0), t.titulo, t.descricao, t.data_entrega, t.concluida, "
                              "COALESCE(u.nome, '') FROM tarefas t LEFT JOIN turmas u ON u.id = t.turma_id"))) {
        while (q.next()) {
            ItemBusca i;
            i.tipo = TipoBusca::Tarefa;
            i.id = q.value(0).toInt();
            i.turmaId = q.value(1).toInt();
            i.titulo = q.value(2).toString();
            i.data = lerData(q.value(4));
            QString contexto = q.value(5).toInt() ? QStringLiteral("Tarefa concluída") : QStringLiteral("Tarefa");
            if (i.data.isValid())
                contexto += QStringLiteral(" · prazo %1").arg(i.data.toString(QStringLiteral("dd/MM/yyyy")));
            if (!q.value(6).toString().isEmpty())
                contexto += QStringLiteral(" · ") + q.value(6).toString();
            i.subtitulo = contexto;
            i.textoNormalizado = indexar({i.titulo, q.value(3).toString()});
            itens.append(i);
        }
    }

    // --- Eventos do calendário ---
    if (q.exec(QStringLiteral("SELECT e.id, COALESCE(e.turma_id, 0), e.titulo, e.tipo, e.data_inicio, e.descricao, "
                              "COALESCE(t.nome, '') FROM eventos e LEFT JOIN turmas t ON t.id = e.turma_id"))) {
        while (q.next()) {
            ItemBusca i;
            i.tipo = TipoBusca::Evento;
            i.id = q.value(0).toInt();
            i.turmaId = q.value(1).toInt();
            i.titulo = q.value(2).toString();
            i.data = lerData(q.value(4));
            i.subtitulo = QStringLiteral("Calendário · %1 · %2").arg(q.value(3).toString(), i.data.toString(QStringLiteral("dd/MM/yyyy")));
            i.textoNormalizado = indexar({i.titulo, q.value(3).toString(), q.value(5).toString(), q.value(6).toString()});
            itens.append(i);
        }
    }

    // --- Anexos (arquivos e apresentações) ---
    if (q.exec(QStringLiteral("SELECT x.id, COALESCE(x.turma_id, 0), x.nome, x.caminho, COALESCE(t.nome, '') "
                              "FROM anexos x LEFT JOIN turmas t ON t.id = x.turma_id"))) {
        while (q.next()) {
            ItemBusca i;
            i.tipo = TipoBusca::Anexo;
            i.id = q.value(0).toInt();
            i.turmaId = q.value(1).toInt();
            i.titulo = q.value(2).toString();
            i.caminho = q.value(3).toString();
            i.subtitulo = QStringLiteral("Arquivo · %1").arg(q.value(4).toString().isEmpty() ? q.value(3).toString()
                                                                                                : q.value(4).toString());
            i.textoNormalizado = indexar({i.titulo, q.value(4).toString()});
            itens.append(i);
        }
    }

    return itens;
}
