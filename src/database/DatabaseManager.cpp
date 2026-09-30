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
