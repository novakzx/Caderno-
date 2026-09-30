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
    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    // WAL deixa a gravação mais robusta contra quedas de energia/travamentos.
    pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"));

    return Migrations::aplicar(db, &m_erro);
}
