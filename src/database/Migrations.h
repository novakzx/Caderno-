#pragma once

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

// Sistema de migrações versionadas.
//
// A versão atual do esquema fica gravada no próprio arquivo SQLite, em
// "PRAGMA user_version". Ao abrir o banco, todas as migrações com versão
// maior que a gravada são aplicadas em ordem, cada uma dentro de uma
// transação (se falhar, nada daquela migração é gravado).
//
// COMO EVOLUIR O ESQUEMA NO FUTURO:
//   1. Nunca edite uma migração que já foi distribuída.
//   2. Adicione uma nova entrada no fim da lista em Migrations.cpp, com
//      versao = (última + 1) e os comandos ALTER TABLE / CREATE TABLE.
namespace Migrations {

struct Migracao {
    int versao;
    QString descricao;
    QStringList comandos;  // um comando SQL por item (SQLite não aceita vários por exec)
};

// Lista de todas as migrações conhecidas, em ordem crescente de versão.
const QList<Migracao> &todas();

// Aplica as migrações pendentes. Retorna false e preenche *erro em caso de falha.
bool aplicar(QSqlDatabase &db, QString *erro);

// Versão do esquema gravada no banco (0 = banco novo).
int versaoAtual(QSqlDatabase &db);

// Executa um comando SQL (sem parâmetros) e o encerra por completo, lendo e
// descartando qualquer linha de resultado. Use para PRAGMAs e comandos soltos.
bool executarComando(QSqlDatabase &db, const QString &sql, QString *erro);

}  // namespace Migrations
