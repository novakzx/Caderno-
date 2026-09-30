#include "services/BackupService.h"

#include "database/DatabaseManager.h"
#include "database/Migrations.h"

#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace BackupService {

namespace {

const char *kPrefixo = "professor-";
const char *kSufixoPendente = ".restaurar";

QString nomeDoBackup(const QDateTime &quando)
{
    return QStringLiteral("%1%2.db").arg(QLatin1String(kPrefixo), quando.toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss")));
}

}  // namespace

QString pastaDeBackups()
{
    const QString pasta = QFileInfo(DatabaseManager::caminhoPadrao()).absolutePath() + QStringLiteral("/backups");
    QDir().mkpath(pasta);
    return pasta;
}

bool criarBackup(const QString &destino, QString *erro)
{
    QDir().mkpath(QFileInfo(destino).absolutePath());
    // VACUUM INTO exige que o arquivo de destino ainda não exista.
    if (QFile::exists(destino) && !QFile::remove(destino)) {
        if (erro)
            *erro = QStringLiteral("Não foi possível substituir o arquivo de destino.");
        return false;
    }

    // VACUUM INTO não aceita parâmetros preparados; o caminho é escapado
    // (aspas simples dobradas) e vem de um diálogo de arquivo ou da nossa pasta.
    QString caminho = QDir::fromNativeSeparators(destino);
    caminho.replace(QLatin1Char('\''), QStringLiteral("''"));

    QSqlQuery q(QSqlDatabase::database());
    if (!q.exec(QStringLiteral("VACUUM INTO '%1'").arg(caminho))) {
        if (erro)
            *erro = q.lastError().text();
        return false;
    }
    return true;
}

QList<QFileInfo> listarBackups()
{
    const QDir pasta(pastaDeBackups());
    return pasta.entryInfoList({QLatin1String(kPrefixo) + QLatin1Char('*') + QStringLiteral(".db")}, QDir::Files,
                               QDir::Time);  // Time: do mais recente para o mais antigo
}

QDateTime ultimoBackup()
{
    const QList<QFileInfo> lista = listarBackups();
    return lista.isEmpty() ? QDateTime() : lista.first().lastModified();
}

bool backupAutomaticoSeNecessario(int intervaloHoras, QString *erro)
{
    const QDateTime ultimo = ultimoBackup();
    if (ultimo.isValid() && ultimo.secsTo(QDateTime::currentDateTime()) < qint64(intervaloHoras) * 3600)
        return false;

    const QString destino = pastaDeBackups() + QLatin1Char('/') + nomeDoBackup(QDateTime::currentDateTime());
    if (!criarBackup(destino, erro))
        return false;

    // Mantém só os mais recentes.
    const QList<QFileInfo> lista = listarBackups();
    for (int i = kMaximoDeBackupsAutomaticos; i < lista.size(); ++i)
        QFile::remove(lista.at(i).absoluteFilePath());
    return true;
}

bool validarArquivo(const QString &arquivo, QString *erro)
{
    auto falha = [erro](const QString &msg) {
        if (erro)
            *erro = msg;
        return false;
    };

    if (!QFile::exists(arquivo))
        return falha(QStringLiteral("O arquivo não existe."));

    // Conexão temporária, só leitura, separada da conexão principal.
    const QString nomeConexao = QStringLiteral("validar_backup_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString problema;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), nomeConexao);
        db.setDatabaseName(arquivo);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));

        if (!db.open()) {
            problema = db.lastError().text();
        } else {
            QSqlQuery q(db);
            if (!q.exec(QStringLiteral("PRAGMA quick_check")) || !q.next()) {
                problema = QStringLiteral("O arquivo não é um banco de dados válido.");
            } else if (q.value(0).toString() != QLatin1String("ok")) {
                problema = QStringLiteral("O arquivo está corrompido (%1).").arg(q.value(0).toString());
            } else if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = 'turmas'")) ||
                       !q.next() || q.value(0).toInt() == 0) {
                problema = QStringLiteral("Este arquivo não parece ser um backup do Caderno+.");
            } else if (q.exec(QStringLiteral("PRAGMA user_version")) && q.next() &&
                       q.value(0).toInt() > Migrations::todas().last().versao) {
                problema = QStringLiteral("O backup foi feito por uma versão mais nova do programa (esquema %1). "
                                          "Atualize o programa para restaurá-lo.")
                               .arg(q.value(0).toInt());
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(nomeConexao);

    if (!problema.isEmpty())
        return falha(problema);
    return true;
}

bool agendarRestauracao(const QString &arquivo, const QString &caminhoBanco, QString *erro)
{
    if (!validarArquivo(arquivo, erro))
        return false;

    const QString pendente = caminhoBanco + QLatin1String(kSufixoPendente);
    QFile::remove(pendente);
    if (!QFile::copy(arquivo, pendente)) {
        if (erro)
            *erro = QStringLiteral("Não foi possível preparar a restauração (sem permissão ou sem espaço em disco).");
        return false;
    }
    return true;
}

bool restauracaoPendente(const QString &caminhoBanco)
{
    return QFile::exists(caminhoBanco + QLatin1String(kSufixoPendente));
}

bool aplicarRestauracaoPendente(const QString &caminhoBanco, QString *erro)
{
    const QString pendente = caminhoBanco + QLatin1String(kSufixoPendente);
    if (!QFile::exists(pendente))
        return false;

    // Revalida: o arquivo pendente pode ter sido alterado desde o agendamento.
    if (!validarArquivo(pendente, erro)) {
        QFile::remove(pendente);
        return false;
    }

    // O banco atual é guardado de lado (nunca apagado), junto dos arquivos do modo WAL.
    if (QFile::exists(caminhoBanco)) {
        const QString guardado = QStringLiteral("%1.antes-da-restauracao-%2")
                                     .arg(caminhoBanco, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
        if (!QFile::rename(caminhoBanco, guardado)) {
            if (erro)
                *erro = QStringLiteral("Não foi possível guardar o banco atual; a restauração foi adiada.");
            return false;
        }
    }
    QFile::remove(caminhoBanco + QStringLiteral("-wal"));
    QFile::remove(caminhoBanco + QStringLiteral("-shm"));

    if (!QFile::rename(pendente, caminhoBanco)) {
        if (erro)
            *erro = QStringLiteral("Não foi possível colocar o backup no lugar do banco.");
        return false;
    }
    return true;
}

}  // namespace BackupService
