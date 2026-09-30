#include "services/ContaService.h"

#include "core/ContaUtil.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <utility>

namespace {

std::function<qint64()> g_relogio;  // vazio = relógio real

qint64 agora()
{
    return g_relogio ? g_relogio() : QDateTime::currentSecsSinceEpoch();
}

const char kMensagemGenerica[] = "E-mail ou senha incorretos.";

QString texto(const std::string &s)
{
    return QString::fromUtf8(s.c_str());
}

// Lê um QString como std::string UTF-8 para as regras puras de ContaUtil.
std::string utf8(const QString &s)
{
    const QByteArray b = s.toUtf8();
    return std::string(b.constData(), static_cast<std::size_t>(b.size()));
}

QString formatarTempo(qint64 segundos)
{
    if (segundos < 60)
        return QStringLiteral("%1 segundos").arg(segundos);
    const qint64 minutos = (segundos + 59) / 60;
    return minutos == 1 ? QStringLiteral("1 minuto") : QStringLiteral("%1 minutos").arg(minutos);
}

}  // namespace

ContaService::ContaService(const QString &pastaBase) : m_pastaBase(pastaBase)
{
    m_repo = std::make_unique<ContaRepository>(QDir(pastaBase).filePath(QStringLiteral("contas.db")));
}

bool ContaService::temContas() const
{
    return disponivel() && m_repo->total() > 0;
}

void ContaService::usarRelogioDeTeste(std::function<qint64()> relogio)
{
    g_relogio = std::move(relogio);
}

QString ContaService::normalizarEmail(const QString &email)
{
    return email.trimmed().toLower();
}

QByteArray ContaService::sorteio(int bytes)
{
    // QRandomGenerator::system() usa o gerador seguro do sistema operacional.
    QByteArray b(bytes, 0);
    for (int i = 0; i < bytes; ++i)
        b[i] = static_cast<char>(QRandomGenerator::system()->bounded(256));
    return b;
}

// Código de recuperação: 20 caracteres (100 bits), sem letras/números que se confundem
// (0/O, 1/I), mostrado em grupos de 4: ABCD-EFGH-JKLM-NPQR-STUV.
QString ContaService::novoCodigoDeRecuperacao()
{
    static const char alfabeto[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";  // 32 símbolos
    QString codigo;
    for (int i = 0; i < 20; ++i) {
        if (i > 0 && i % 4 == 0)
            codigo += QLatin1Char('-');
        codigo += QLatin1Char(alfabeto[QRandomGenerator::system()->bounded(32)]);
    }
    return codigo;
}

QByteArray ContaService::normalizarCodigo(const QString &codigo)
{
    QString limpo;
    for (const QChar c : codigo)
        if (c.isLetterOrNumber())
            limpo += c.toUpper();
    return limpo.toUtf8();
}

// PBKDF2 (RFC 8018) com HMAC-SHA512, para uma única saída de até 64 bytes (um bloco).
QByteArray ContaService::pbkdf2(const QByteArray &senha, const QByteArray &sal, int iteracoes, int tamanho)
{
    QMessageAuthenticationCode mac(QCryptographicHash::Sha512, senha);
    QByteArray bloco = sal;
    bloco.append(char(0)).append(char(0)).append(char(0)).append(char(1));  // INT(1), 4 bytes
    mac.addData(bloco);
    QByteArray u = mac.result();
    QByteArray t = u;
    for (int i = 1; i < iteracoes; ++i) {
        mac.reset();  // mantém a chave
        mac.addData(u);
        u = mac.result();
        for (int k = 0; k < t.size(); ++k)
            t[k] = static_cast<char>(t[k] ^ u[k]);
    }
    return t.left(tamanho);
}

QString ContaService::caminhoDosDados(const Conta &conta) const
{
    return QDir(m_pastaBase).filePath(conta.arquivoDados);
}

ResultadoConta ContaService::registrar(const QString &nome, const QString &email, const QString &senha,
                                       const QString &confirmacao)
{
    ResultadoConta r;
    if (!disponivel()) {
        r.erro = QStringLiteral("Não foi possível abrir o cadastro de contas: %1").arg(erroDeAbertura());
        return r;
    }

    const QString emailNormalizado = normalizarEmail(email);
    const QString nomeAparado = nome.trimmed();

    if (const auto v = ContaUtil::validarNome(utf8(nomeAparado)); !v.ok) {
        r.erro = texto(v.motivo);
        return r;
    }
    if (const auto v = ContaUtil::validarEmail(utf8(emailNormalizado)); !v.ok) {
        r.erro = texto(v.motivo);
        return r;
    }
    if (const auto v = ContaUtil::validarSenha(utf8(senha), utf8(nomeAparado), utf8(emailNormalizado)); !v.ok) {
        r.erro = texto(v.motivo);
        return r;
    }
    if (senha != confirmacao) {
        r.erro = QStringLiteral("A confirmação da senha não confere.");
        return r;
    }
    if (m_repo->porEmail(emailNormalizado)) {
        r.erro = QStringLiteral("Já existe uma conta com esse e-mail. Use \"Entrar\".");
        return r;
    }

    ContaRegistro c;
    c.nome = nomeAparado;
    c.email = emailNormalizado;
    c.iteracoes = kIteracoes;
    c.sal = sorteio(16);
    c.hash = pbkdf2(senha.toUtf8(), c.sal, c.iteracoes);
    const QString codigo = novoCodigoDeRecuperacao();
    c.iteracoesRecuperacao = kIteracoes;
    c.salRecuperacao = sorteio(16);
    c.hashRecuperacao = pbkdf2(normalizarCodigo(codigo), c.salRecuperacao, c.iteracoesRecuperacao);

    // A primeira conta adota o banco que já existia (professor.db); as seguintes têm pasta própria.
    const bool primeira = m_repo->total() == 0;
    c.arquivoDados = primeira ? QStringLiteral("professor.db") : QString();
    const int id = m_repo->inserir(c);
    if (id == 0) {
        r.erro = QStringLiteral("Não foi possível criar a conta: %1").arg(m_repo->ultimoErro());
        return r;
    }
    c.id = id;
    if (!primeira) {
        c.arquivoDados = QStringLiteral("contas/%1/professor.db").arg(id);
        if (!m_repo->atualizarArquivoDados(id, c.arquivoDados)) {
            r.erro = QStringLiteral("Não foi possível criar a conta: %1").arg(m_repo->ultimoErro());
            return r;
        }
    }
    m_repo->registrarAcesso(id);

    r.ok = true;
    r.conta = {c.id, c.nome, c.email, c.arquivoDados};
    r.codigoRecuperacao = codigo;
    return r;
}

// Conta uma falha, aplica o bloqueio (se for o caso) e devolve a mensagem de erro.
ResultadoConta ContaService::falhaDeAcesso(ContaRegistro &conta, const QString &mensagemBase)
{
    ResultadoConta r;
    const int falhas = conta.falhasSeguidas + 1;
    const qint64 bloqueio = ContaUtil::segundosDeBloqueio(falhas);
    m_repo->registrarFalhas(conta.id, falhas, bloqueio > 0 ? agora() + bloqueio : 0);
    r.segundosDeBloqueio = bloqueio;
    r.erro = bloqueio > 0 ? QStringLiteral("%1 Muitas tentativas: aguarde %2 para tentar de novo.")
                                .arg(mensagemBase, formatarTempo(bloqueio))
                          : mensagemBase;
    return r;
}

ResultadoConta ContaService::entrar(const QString &email, const QString &senha)
{
    ResultadoConta r;
    if (!disponivel()) {
        r.erro = QStringLiteral("Não foi possível abrir o cadastro de contas: %1").arg(erroDeAbertura());
        return r;
    }

    auto achada = m_repo->porEmail(normalizarEmail(email));
    if (!achada) {
        // Gasta o mesmo tempo de uma conta real, para não revelar (pelo tempo) quais e-mails existem.
        pbkdf2(senha.toUtf8(), QByteArray(16, 'x'), kIteracoes);
        r.erro = QString::fromUtf8(kMensagemGenerica);
        return r;
    }

    ContaRegistro &conta = *achada;
    if (conta.bloqueadoAte > agora()) {
        r.segundosDeBloqueio = conta.bloqueadoAte - agora();
        r.erro = QStringLiteral("Muitas tentativas erradas. Aguarde %1 para tentar de novo.")
                     .arg(formatarTempo(r.segundosDeBloqueio));
        return r;
    }

    const QByteArray calculado = pbkdf2(senha.toUtf8(), conta.sal, conta.iteracoes);
    if (!ContaUtil::iguaisEmTempoConstante(std::string(calculado.constData(), calculado.size()),
                                           std::string(conta.hash.constData(), conta.hash.size())))
        return falhaDeAcesso(conta, QString::fromUtf8(kMensagemGenerica));

    m_repo->registrarAcesso(conta.id);
    r.ok = true;
    r.conta = {conta.id, conta.nome, conta.email, conta.arquivoDados};
    return r;
}

ResultadoConta ContaService::redefinirSenha(const QString &email, const QString &codigoRecuperacao,
                                            const QString &novaSenha)
{
    ResultadoConta r;
    if (!disponivel()) {
        r.erro = QStringLiteral("Não foi possível abrir o cadastro de contas: %1").arg(erroDeAbertura());
        return r;
    }
    const QString mensagem = QStringLiteral("E-mail ou código de recuperação incorretos.");

    auto achada = m_repo->porEmail(normalizarEmail(email));
    if (!achada) {
        pbkdf2(codigoRecuperacao.toUtf8(), QByteArray(16, 'x'), kIteracoes);
        r.erro = mensagem;
        return r;
    }
    ContaRegistro &conta = *achada;
    if (conta.bloqueadoAte > agora()) {
        r.segundosDeBloqueio = conta.bloqueadoAte - agora();
        r.erro = QStringLiteral("Muitas tentativas erradas. Aguarde %1 para tentar de novo.")
                     .arg(formatarTempo(r.segundosDeBloqueio));
        return r;
    }

    const QByteArray calculado = pbkdf2(normalizarCodigo(codigoRecuperacao), conta.salRecuperacao, conta.iteracoesRecuperacao);
    if (!ContaUtil::iguaisEmTempoConstante(std::string(calculado.constData(), calculado.size()),
                                           std::string(conta.hashRecuperacao.constData(), conta.hashRecuperacao.size())))
        return falhaDeAcesso(conta, mensagem);

    if (const auto v = ContaUtil::validarSenha(utf8(novaSenha), utf8(conta.nome), utf8(conta.email)); !v.ok) {
        r.erro = texto(v.motivo);
        return r;
    }

    // Senha nova + código de recuperação NOVO (o antigo deixa de valer).
    const QByteArray sal = sorteio(16);
    const QByteArray hash = pbkdf2(novaSenha.toUtf8(), sal, kIteracoes);
    const QString codigo = novoCodigoDeRecuperacao();
    const QByteArray salRec = sorteio(16);
    const QByteArray hashRec = pbkdf2(normalizarCodigo(codigo), salRec, kIteracoes);
    if (!m_repo->atualizarSenha(conta.id, sal, hash, kIteracoes, salRec, hashRec, kIteracoes)) {
        r.erro = QStringLiteral("Não foi possível salvar a nova senha: %1").arg(m_repo->ultimoErro());
        return r;
    }

    r.ok = true;
    r.conta = {conta.id, conta.nome, conta.email, conta.arquivoDados};
    r.codigoRecuperacao = codigo;
    return r;
}
