#pragma once

#include "database/ContaRepository.h"

#include <QByteArray>
#include <QString>

#include <functional>
#include <memory>

// Dados da conta que entrou (sem nada secreto).
struct Conta {
    int id = 0;
    QString nome;
    QString email;
    QString arquivoDados;  // arquivo do banco de dados desta conta, relativo à pasta de dados
};

struct ResultadoConta {
    bool ok = false;
    QString erro;                  // mensagem para o usuário quando !ok
    Conta conta;
    QString codigoRecuperacao;     // só ao criar a conta ou redefinir a senha: mostrar UMA vez
    qint64 segundosDeBloqueio = 0; // > 0 quando a conta está bloqueada por tentativas erradas
};

// Contas locais do programa (login e cadastro, sem internet).
//
//  - A senha é guardada como PBKDF2-HMAC-SHA512 (210.000 iterações, sal aleatório de 16 bytes);
//    nunca em texto. O mesmo vale para o código de recuperação (100 bits aleatórios).
//  - Depois de 5 senhas erradas seguidas a conta é bloqueada por um tempo que cresce
//    (30 s, 1 min, 2 min... até 15 min). Mensagens de erro iguais para e-mail inexistente e
//    senha errada (não revela quais e-mails têm conta).
//  - Cada conta tem o próprio arquivo de dados. A primeira conta criada adota o banco que
//    já existia (professor.db), para ninguém perder o que já cadastrou.
//
// Limite: protege o acesso ao programa, mas o arquivo do banco não é criptografado.
class ContaService {
public:
    static constexpr int kIteracoes = 210000;

    // `pastaBase` = pasta de dados do programa (onde ficam contas.db e professor.db).
    explicit ContaService(const QString &pastaBase);

    bool disponivel() const { return m_repo && m_repo->aberto(); }
    QString erroDeAbertura() const { return m_repo ? m_repo->ultimoErro() : QString(); }
    bool temContas() const;

    ResultadoConta registrar(const QString &nome, const QString &email, const QString &senha,
                             const QString &confirmacao);
    ResultadoConta entrar(const QString &email, const QString &senha);
    ResultadoConta redefinirSenha(const QString &email, const QString &codigoRecuperacao, const QString &novaSenha);

    // Caminho completo do banco de dados da conta.
    QString caminhoDosDados(const Conta &conta) const;

    // PBKDF2-HMAC-SHA512 (público para os testes conferirem com vetores conhecidos).
    static QByteArray pbkdf2(const QByteArray &senha, const QByteArray &sal, int iteracoes, int tamanho = 64);

    // Só para testes: troca o relógio (segundos desde 1970) para simular a passagem do tempo.
    static void usarRelogioDeTeste(std::function<qint64()> relogio);

private:
    ResultadoConta falhaDeAcesso(ContaRegistro &conta, const QString &mensagemBase);
    static QString normalizarEmail(const QString &email);
    static QByteArray sorteio(int bytes);
    static QString novoCodigoDeRecuperacao();
    static QByteArray normalizarCodigo(const QString &codigo);

    QString m_pastaBase;
    mutable std::unique_ptr<ContaRepository> m_repo;
};
