#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

// Registro de uma conta como está no banco de contas (contas.db). A senha NUNCA é
// guardada: só o resultado do PBKDF2 (hash) e o sal. O mesmo vale para o código de recuperação.
struct ContaRegistro {
    int id = 0;
    QString nome;
    QString email;          // sempre em minúsculas
    QString arquivoDados;   // arquivo do banco de dados desta conta, relativo à pasta de dados
    QByteArray sal;
    QByteArray hash;
    int iteracoes = 0;
    QByteArray salRecuperacao;
    QByteArray hashRecuperacao;
    int iteracoesRecuperacao = 0;
    int falhasSeguidas = 0;
    qint64 bloqueadoAte = 0;  // segundos desde 1970; 0 = não bloqueada
};

// Banco das contas locais (arquivo contas.db, separado dos dados de cada professor).
// Único lugar com SQL das contas. Usa uma conexão própria (não a padrão do Qt), então
// funciona antes de o banco de dados do professor ser aberto.
class ContaRepository {
public:
    explicit ContaRepository(const QString &arquivo);
    ~ContaRepository();
    ContaRepository(const ContaRepository &) = delete;
    ContaRepository &operator=(const ContaRepository &) = delete;

    bool aberto() const { return m_aberto; }
    QString ultimoErro() const { return m_erro; }

    int total();
    std::optional<ContaRegistro> porEmail(const QString &email);

    // Devolve o id da conta criada, ou 0 em caso de erro (por exemplo, e-mail repetido).
    int inserir(const ContaRegistro &conta);
    bool atualizarArquivoDados(int id, const QString &arquivoDados);
    bool atualizarSenha(int id, const QByteArray &sal, const QByteArray &hash, int iteracoes,
                        const QByteArray &salRecuperacao, const QByteArray &hashRecuperacao, int iteracoesRecuperacao);
    bool registrarFalhas(int id, int falhasSeguidas, qint64 bloqueadoAte);
    bool registrarAcesso(int id);  // zera as falhas e marca o último acesso

private:
    ContaRegistro ler(class QSqlQuery &q) const;
    bool criarEsquema();

    QString m_conexao;
    QString m_erro;
    bool m_aberto = false;
};
