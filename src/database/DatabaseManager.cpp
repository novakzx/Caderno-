#include "database/DatabaseManager.h"

#include "database/Migrations.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

QString DatabaseManager::caminhoPadrao()
{
    const QString pasta = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(pasta).filePath(QStringLiteral("professor.db"));
}

namespace {
QString g_caminhoAtual;  // vazio = caminho padrão
}

QString DatabaseManager::caminhoAtual()
{
    return g_caminhoAtual.isEmpty() ? caminhoPadrao() : g_caminhoAtual;
}

void DatabaseManager::definirCaminhoAtual(const QString &caminho)
{
    g_caminhoAtual = caminho;
}

DatabaseManager::~DatabaseManager()
{
    const QString conexao = QLatin1String(QSqlDatabase::defaultConnection);
    if (!QSqlDatabase::contains(conexao))
        return;
    {
        QSqlDatabase db = QSqlDatabase::database(conexao, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(conexao);
}

bool DatabaseManager::abrir(const QString &caminho)
{
    // Garante que a pasta existe.
    QDir().mkpath(QFileInfo(caminho).absolutePath());

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(caminho);
    if (!db.open()) {
        m_erro = db.lastError().text();
        return false;
    }

    // O SQLite NÃO aplica chaves estrangeiras por padrão: precisa ligar a cada
    // conexão. Sem isso, ON DELETE CASCADE não funcionaria.
    Migrations::executarComando(db, QStringLiteral("PRAGMA foreign_keys = ON"), nullptr);

    // Segurança: um banco que veio de fora (backup restaurado) não pode usar views/gatilhos para chamar
    // funções SQL perigosas; e dados apagados (ex.: um aluno excluído) são sobrescritos no arquivo
    // em vez de ficarem "esquecidos" nele.
    Migrations::executarComando(db, QStringLiteral("PRAGMA trusted_schema = OFF"), nullptr);
    Migrations::executarComando(db, QStringLiteral("PRAGMA secure_delete = ON"), nullptr);

    // Primeiro as migrações (criam/atualizam as tabelas)...
    if (!Migrations::aplicar(db, &m_erro))
        return false;

    // ...e só depois o modo WAL, que deixa a gravação mais robusta contra quedas de
    // energia/travamentos. "PRAGMA journal_mode" devolve uma linha de resultado; o
    // SQLite recusa o COMMIT de uma transação enquanto houver um comando "em
    // andamento" na conexão, por isso este PRAGMA fica fora das migrações e é
    // lido até o fim por executarComando(). Se falhar, o app segue no modo padrão.
    Migrations::executarComando(db, QStringLiteral("PRAGMA journal_mode = WAL"), nullptr);
    return true;
}
