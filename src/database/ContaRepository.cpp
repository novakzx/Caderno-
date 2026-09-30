#include "database/ContaRepository.h"

#include "database/SqlUtil.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

ContaRepository::ContaRepository(const QString &arquivo)
{
    QDir().mkpath(QFileInfo(arquivo).absolutePath());

    // Nome de conexão único: pode haver mais de um repositório aberto (por exemplo, nos testes).
    m_conexao = QStringLiteral("contas_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_conexao);
    db.setDatabaseName(arquivo);
    if (!db.open()) {
        m_erro = db.lastError().text();
        return;
    }
    m_aberto = criarEsquema();
}

ContaRepository::~ContaRepository()
{
    {
        QSqlDatabase db = QSqlDatabase::database(m_conexao, false);
        if (db.isValid() && db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_conexao);
}

bool ContaRepository::criarEsquema()
{
    QSqlDatabase db = QSqlDatabase::database(m_conexao);
    QSqlQuery q(db);
    const bool ok = q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS contas ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nome TEXT NOT NULL,"
        " email TEXT NOT NULL UNIQUE COLLATE NOCASE,"
        " arquivo_dados TEXT NOT NULL DEFAULT '',"
        " sal BLOB NOT NULL, hash BLOB NOT NULL, iteracoes INTEGER NOT NULL,"
        " sal_rec BLOB NOT NULL, hash_rec BLOB NOT NULL, iteracoes_rec INTEGER NOT NULL,"
        " falhas_seguidas INTEGER NOT NULL DEFAULT 0,"
        " bloqueado_ate INTEGER NOT NULL DEFAULT 0,"
        " criada_em TEXT NOT NULL DEFAULT (datetime('now')),"
        " ultimo_acesso TEXT)"));
    if (!ok)
        m_erro = q.lastError().text();
    q.finish();
    return ok;
}

int ContaRepository::total()
{
    QSqlQuery q(QSqlDatabase::database(m_conexao));
    int n = 0;
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM contas")) && q.next())
        n = q.value(0).toInt();
    q.finish();
    return n;
}

ContaRegistro ContaRepository::ler(QSqlQuery &q) const
{
    ContaRegistro c;
    c.id = q.value(0).toInt();
    c.nome = q.value(1).toString();
    c.email = q.value(2).toString();
    c.arquivoDados = q.value(3).toString();
    c.sal = q.value(4).toByteArray();
    c.hash = q.value(5).toByteArray();
    c.iteracoes = q.value(6).toInt();
    c.salRecuperacao = q.value(7).toByteArray();
    c.hashRecuperacao = q.value(8).toByteArray();
    c.iteracoesRecuperacao = q.value(9).toInt();
    c.falhasSeguidas = q.value(10).toInt();
    c.bloqueadoAte = q.value(11).toLongLong();
    return c;
}

static const char *kColunas =
    "id, nome, email, arquivo_dados, sal, hash, iteracoes, sal_rec, hash_rec, iteracoes_rec, "
    "falhas_seguidas, bloqueado_ate";

std::optional<ContaRegistro> ContaRepository::porEmail(const QString &email)
{
    QSqlQuery q(QSqlDatabase::database(m_conexao));
    q.prepare(QStringLiteral("SELECT %1 FROM contas WHERE email = :email").arg(QLatin1String(kColunas)));
    ligar(q, QStringLiteral(":email"), email);
    std::optional<ContaRegistro> resultado;
    if (q.exec() && q.next())
        resultado = ler(q);
    else if (q.lastError().isValid())
        m_erro = q.lastError().text();
    q.finish();
    return resultado;
}

int ContaRepository::inserir(const ContaRegistro &c)
{
    QSqlQuery q(QSqlDatabase::database(m_conexao));
    q.prepare(QStringLiteral(
        "INSERT INTO contas (nome, email, arquivo_dados, sal, hash, iteracoes, sal_rec, hash_rec, iteracoes_rec) "
        "VALUES (:nome, :email, :arquivo, :sal, :hash, :it, :salrec, :hashrec, :itrec)"));
    ligar(q, QStringLiteral(":nome"), c.nome);
    ligar(q, QStringLiteral(":email"), c.email);
    ligar(q, QStringLiteral(":arquivo"), c.arquivoDados);
    q.bindValue(QStringLiteral(":sal"), c.sal);
    q.bindValue(QStringLiteral(":hash"), c.hash);
    q.bindValue(QStringLiteral(":it"), c.iteracoes);
    q.bindValue(QStringLiteral(":salrec"), c.salRecuperacao);
    q.bindValue(QStringLiteral(":hashrec"), c.hashRecuperacao);
    q.bindValue(QStringLiteral(":itrec"), c.iteracoesRecuperacao);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return 0;
    }
    const int id = q.lastInsertId().toInt();
    q.finish();
    return id;
}

bool ContaRepository::atualizarArquivoDados(int id, const QString &arquivoDados)
{
    QSqlQuery q(QSqlDatabase::database(m_conexao));
    q.prepare(QStringLiteral("UPDATE contas SET arquivo_dados = :a WHERE id = :id"));
    ligar(q, QStringLiteral(":a"), arquivoDados);
    q.bindValue(QStringLiteral(":id"), id);
    const bool ok = q.exec();
    if (!ok)
        m_erro = q.lastError().text();
    q.finish();
    return ok;
}

bool ContaRepository::atualizarSenha(int id, const QByteArray &sal, const QByteArray &hash, int iteracoes,
                                     const QByteArray &salRecuperacao, const QByteArray &hashRecuperacao,
                                     int iteracoesRecuperacao)
{
    QSqlQuery q(QSqlDatabase::database(m_conexao));
    q.prepare(QStringLiteral(
        "UPDATE contas SET sal = :sal, hash = :hash, iteracoes = :it, sal_rec = :salrec, hash_rec = :hashrec, "
        "iteracoes_rec = :itrec, falhas_seguidas = 0, bloqueado_ate = 0 WHERE id = :id"));
    q.bindValue(QStringLiteral(":sal"), sal);
    q.bindValue(QStringLiteral(":hash"), hash);
    q.bindValue(QStringLiteral(":it"), iteracoes);
    q.bindValue(QStringLiteral(":salrec"), salRecuperacao);
    q.bindValue(QStringLiteral(":hashrec"), hashRecuperacao);
    q.bindValue(QStringLiteral(":itrec"), iteracoesRecuperacao);
    q.bindValue(QStringLiteral(":id"), id);
    const bool ok = q.exec();
    if (!ok)
        m_erro = q.lastError().text();
    q.finish();
    return ok;
}

bool ContaRepository::registrarFalhas(int id, int falhasSeguidas, qint64 bloqueadoAte)
{
    QSqlQuery q(QSqlDatabase::database(m_conexao));
    q.prepare(QStringLiteral("UPDATE contas SET falhas_seguidas = :f, bloqueado_ate = :b WHERE id = :id"));
    q.bindValue(QStringLiteral(":f"), falhasSeguidas);
    q.bindValue(QStringLiteral(":b"), bloqueadoAte);
    q.bindValue(QStringLiteral(":id"), id);
    const bool ok = q.exec();
    if (!ok)
        m_erro = q.lastError().text();
    q.finish();
    return ok;
}

bool ContaRepository::registrarAcesso(int id)
{
    QSqlQuery q(QSqlDatabase::database(m_conexao));
    q.prepare(QStringLiteral(
        "UPDATE contas SET falhas_seguidas = 0, bloqueado_ate = 0, ultimo_acesso = datetime('now') WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    const bool ok = q.exec();
    if (!ok)
        m_erro = q.lastError().text();
    q.finish();
    return ok;
}
