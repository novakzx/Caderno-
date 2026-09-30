#include "database/AnotacaoRepository.h"

#include "database/SqlUtil.h"

#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace {

// Limpa a lista de tags: sem "#", sem vírgulas, sem vazias, sem repetidas
// (ignorando maiúsculas/minúsculas).
QStringList normalizarTags(const QStringList &tags)
{
    QStringList resultado;
    QSet<QString> vistas;
    for (QString t : tags) {
        t.remove(QLatin1Char('#'));
        t.replace(QLatin1Char(','), QLatin1Char(' '));
        t = t.simplified();
        if (t.isEmpty() || vistas.contains(t.toLower()))
            continue;
        vistas.insert(t.toLower());
        resultado.append(t);
    }
    return resultado;
}

}  // namespace

QList<AnotacaoResumo> AnotacaoRepository::listar(const Filtro &f, int limite)
{
    QList<AnotacaoResumo> resultado;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT a.id, a.titulo, a.atualizada_em, COALESCE(t.nome, ''), COALESCE(al.nome, ''), "
        "substr(a.conteudo_texto, 1, 140), "
        "COALESCE((SELECT group_concat(g.nome, ',') FROM anotacao_tags x "
        "          JOIN tags g ON g.id = x.tag_id WHERE x.anotacao_id = a.id), '') "
        "FROM anotacoes a "
        "LEFT JOIN turmas t ON t.id = a.turma_id "
        "LEFT JOIN alunos al ON al.id = a.aluno_id "
        "WHERE (:turma = 0 OR a.turma_id = :turma) "
        "AND (:aluno = 0 OR a.aluno_id = :aluno) "
        "AND (:tag = '' OR EXISTS (SELECT 1 FROM anotacao_tags x JOIN tags g ON g.id = x.tag_id "
        "                          WHERE x.anotacao_id = a.id AND g.nome = :tag)) "
        "AND (:texto = '' OR a.titulo LIKE :padrao OR a.conteudo_texto LIKE :padrao) "
        "ORDER BY a.atualizada_em DESC, a.id DESC LIMIT :limite"));
    q.bindValue(QStringLiteral(":turma"), f.turmaId);
    q.bindValue(QStringLiteral(":aluno"), f.alunoId);
    q.bindValue(QStringLiteral(":tag"), f.tag);
    q.bindValue(QStringLiteral(":texto"), f.texto.trimmed());
    q.bindValue(QStringLiteral(":padrao"), QStringLiteral("%") + f.texto.trimmed() + QStringLiteral("%"));
    q.bindValue(QStringLiteral(":limite"), limite);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return resultado;
    }
    while (q.next()) {
        AnotacaoResumo r;
        r.id = q.value(0).toInt();
        r.titulo = q.value(1).toString();
        r.atualizadaEm = q.value(2).toString();
        r.turmaNome = q.value(3).toString();
        r.alunoNome = q.value(4).toString();
        r.trecho = q.value(5).toString().simplified();
        const QString tags = q.value(6).toString();
        r.tags = tags.isEmpty() ? QStringList() : tags.split(QLatin1Char(','));
        resultado.append(r);
    }
    return resultado;
}

std::optional<Anotacao> AnotacaoRepository::buscar(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT id, titulo, conteudo_html, conteudo_texto, COALESCE(turma_id, 0), COALESCE(aluno_id, 0), "
        "COALESCE(aula_id, 0), criada_em, atualizada_em FROM anotacoes WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return std::nullopt;
    }
    if (!q.next())
        return std::nullopt;

    Anotacao a;
    a.id = q.value(0).toInt();
    a.titulo = q.value(1).toString();
    a.conteudoHtml = q.value(2).toString();
    a.conteudoTexto = q.value(3).toString();
    a.turmaId = q.value(4).toInt();
    a.alunoId = q.value(5).toInt();
    a.aulaId = q.value(6).toInt();
    a.criadaEm = q.value(7).toString();
    a.atualizadaEm = q.value(8).toString();

    QSqlQuery qt;
    qt.prepare(QStringLiteral("SELECT g.nome FROM anotacao_tags x JOIN tags g ON g.id = x.tag_id "
                              "WHERE x.anotacao_id = :id ORDER BY g.nome COLLATE NOCASE"));
    qt.bindValue(QStringLiteral(":id"), id);
    if (qt.exec()) {
        while (qt.next())
            a.tags.append(qt.value(0).toString());
    }
    return a;
}

int AnotacaoRepository::inserir(const Anotacao &a)
{
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO anotacoes (titulo, conteudo_html, conteudo_texto, turma_id, aluno_id, aula_id, "
        "criada_em, atualizada_em) VALUES (:titulo, :html, :texto, :turma, :aluno, :aula, "
        "datetime('now', 'localtime'), datetime('now', 'localtime'))"));
    q.bindValue(QStringLiteral(":titulo"), a.titulo);
    q.bindValue(QStringLiteral(":html"), a.conteudoHtml);
    q.bindValue(QStringLiteral(":texto"), a.conteudoTexto);
    q.bindValue(QStringLiteral(":turma"), nuloSeZero(a.turmaId));
    q.bindValue(QStringLiteral(":aluno"), nuloSeZero(a.alunoId));
    q.bindValue(QStringLiteral(":aula"), nuloSeZero(a.aulaId));

    if (!q.exec()) {
        m_erro = q.lastError().text();
        db.rollback();
        return 0;
    }
    const int id = q.lastInsertId().toInt();
    if (!definirTags(id, a.tags)) {
        db.rollback();
        return 0;
    }
    if (!db.commit()) {
        m_erro = db.lastError().text();
        return 0;
    }
    return id;
}

bool AnotacaoRepository::atualizar(const Anotacao &a)
{
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery q;
    q.prepare(QStringLiteral(
        "UPDATE anotacoes SET titulo = :titulo, conteudo_html = :html, conteudo_texto = :texto, "
        "turma_id = :turma, aluno_id = :aluno, aula_id = :aula, "
        "atualizada_em = datetime('now', 'localtime') WHERE id = :id"));
    q.bindValue(QStringLiteral(":titulo"), a.titulo);
    q.bindValue(QStringLiteral(":html"), a.conteudoHtml);
    q.bindValue(QStringLiteral(":texto"), a.conteudoTexto);
    q.bindValue(QStringLiteral(":turma"), nuloSeZero(a.turmaId));
    q.bindValue(QStringLiteral(":aluno"), nuloSeZero(a.alunoId));
    q.bindValue(QStringLiteral(":aula"), nuloSeZero(a.aulaId));
    q.bindValue(QStringLiteral(":id"), a.id);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        db.rollback();
        return false;
    }
    if (!definirTags(a.id, a.tags)) {
        db.rollback();
        return false;
    }
    if (!db.commit()) {
        m_erro = db.lastError().text();
        return false;
    }
    return true;
}

bool AnotacaoRepository::remover(int id)
{
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM anotacoes WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        db.rollback();
        return false;
    }
    // Tags que ficaram sem nenhuma anotação são apagadas.
    QSqlQuery limpeza;
    limpeza.exec(QStringLiteral("DELETE FROM tags WHERE id NOT IN (SELECT tag_id FROM anotacao_tags)"));
    return db.commit();
}

QStringList AnotacaoRepository::listarTags()
{
    QStringList tags;
    QSqlQuery q;
    if (!q.exec(QStringLiteral("SELECT nome FROM tags ORDER BY nome COLLATE NOCASE"))) {
        m_erro = q.lastError().text();
        return tags;
    }
    while (q.next())
        tags.append(q.value(0).toString());
    return tags;
}

// Substitui as tags da anotação (dentro da transação do chamador).
bool AnotacaoRepository::definirTags(int anotacaoId, const QStringList &tags)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM anotacao_tags WHERE anotacao_id = :id"));
    q.bindValue(QStringLiteral(":id"), anotacaoId);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }

    for (const QString &tag : normalizarTags(tags)) {
        QSqlQuery inserir;
        inserir.prepare(QStringLiteral("INSERT OR IGNORE INTO tags (nome) VALUES (:nome)"));
        inserir.bindValue(QStringLiteral(":nome"), tag);
        if (!inserir.exec()) {
            m_erro = inserir.lastError().text();
            return false;
        }

        // A coluna "nome" é COLLATE NOCASE: "Prova" e "prova" são a mesma tag.
        QSqlQuery vincular;
        vincular.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO anotacao_tags (anotacao_id, tag_id) "
            "SELECT :anot, id FROM tags WHERE nome = :nome"));
        vincular.bindValue(QStringLiteral(":anot"), anotacaoId);
        vincular.bindValue(QStringLiteral(":nome"), tag);
        if (!vincular.exec()) {
            m_erro = vincular.lastError().text();
            return false;
        }
    }

    // Tags órfãs (sem nenhuma anotação) saem da lista.
    QSqlQuery limpeza;
    limpeza.exec(QStringLiteral("DELETE FROM tags WHERE id NOT IN (SELECT tag_id FROM anotacao_tags)"));
    return true;
}
