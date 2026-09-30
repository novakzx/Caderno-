#pragma once

#include <QString>

// Abre o arquivo SQLite, liga as chaves estrangeiras e aplica as migrações.
// Usa a conexão padrão do Qt (QSqlDatabase::database()), que os repositórios
// consomem.
class DatabaseManager {
public:
    // Caminho padrão do arquivo: pasta de dados do aplicativo do usuário
    // (Windows: %APPDATA%/ProfOrganizer/ProfOrganizer/professor.db).
    static QString caminhoPadrao();

    // Banco da conta que está usando o programa (cada conta tem o seu arquivo). Enquanto
    // nenhuma conta definir o caminho, vale o caminho padrão.
    static QString caminhoAtual();
    static void definirCaminhoAtual(const QString &caminho);

    DatabaseManager() = default;
    ~DatabaseManager();  // fecha a conexão (necessário para trocar de conta)
    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;

    // Abre (criando se necessário) e migra o banco. Retorna false em caso de erro.
    bool abrir(const QString &caminho);

    QString ultimoErro() const { return m_erro; }

private:
    QString m_erro;
};
