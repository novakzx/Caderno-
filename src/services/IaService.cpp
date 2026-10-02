#include "services/IaService.h"

#include "core/BuildInfo.h"
#include "services/SegredoService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>

namespace {

const char *kBaseDaApi = "https://api.cloudflare.com/client/v4";
constexpr qint64 kTamanhoMaximoDoCorpoBruto = 1024 * 1024;  // guardado só para o caso de resposta sem fluxo / erro
constexpr int kMaximoDeCaracteres = 120000;                 // teto do texto gerado
constexpr int kTempoLimiteMs = 120 * 1000;

QString g_servidorDeTeste;  // vazio = produção

QString baseAtual()
{
    return g_servidorDeTeste.isEmpty() ? QString::fromLatin1(kBaseDaApi) : g_servidorDeTeste;
}

QString textoCurto(QString texto, int limite = 240)
{
    texto = texto.simplified();
    if (texto.size() > limite)
        texto = texto.left(limite) + QStringLiteral("…");
    return texto;
}

// Extrai o texto de um objeto JSON de resposta, em vários formatos conhecidos.
QString textoDoObjeto(const QJsonObject &o)
{
    if (o.contains(QLatin1String("response")))  // Workers AI (fluxo): {"response":"..."}
        return o.value(QLatin1String("response")).toString();
    const QJsonObject resultado = o.value(QLatin1String("result")).toObject();  // resposta completa: {"result":{"response":"..."}}
    if (resultado.contains(QLatin1String("response")))
        return resultado.value(QLatin1String("response")).toString();
    const QJsonArray escolhas = o.value(QLatin1String("choices")).toArray();  // formato "compatível com OpenAI"
    if (!escolhas.isEmpty()) {
        const QJsonObject escolha = escolhas.first().toObject();
        const QJsonObject delta = escolha.value(QLatin1String("delta")).toObject();
        if (delta.contains(QLatin1String("content")))
            return delta.value(QLatin1String("content")).toString();
        return escolha.value(QLatin1String("message")).toObject().value(QLatin1String("content")).toString();
    }
    return QString();
}

}  // namespace

bool ConfigIa::completa() const
{
    return IaService::accountIdValido(accountId) && IaService::tokenValido(token) && IaService::modeloValido(modelo);
}

IaService::IaService(QObject *parent) : QObject(parent), m_rede(new QNetworkAccessManager(this)) {}

IaService::~IaService()
{
    if (m_resposta) {
        m_resposta->disconnect(this);
        m_resposta->abort();
        m_resposta->deleteLater();
    }
}

QString IaService::modeloPadrao()
{
    return QStringLiteral("@cf/meta/llama-3.1-8b-instruct-fp8");
}

QStringList IaService::modelosSugeridos()
{
    return {
        QStringLiteral("@cf/meta/llama-3.1-8b-instruct-fp8"),        // rápido e econômico (padrão)
        QStringLiteral("@cf/meta/llama-3.3-70b-instruct-fp8-fast"),  // respostas melhores, gasta a cota diária bem mais rápido
    };
}

bool IaService::accountIdValido(const QString &id)
{
    static const QRegularExpression padrao(QStringLiteral("^[0-9a-fA-F]{32}$"));
    return padrao.match(id).hasMatch();
}

bool IaService::tokenValido(const QString &token)
{
    // Tokens do Cloudflare são letras, números, "-" e "_". Qualquer outra coisa (espaço, quebra de linha) é recusada:
    // o token vai num cabeçalho HTTP e não pode carregar nada além dele.
    static const QRegularExpression padrao(QStringLiteral("^[A-Za-z0-9_\\-]{20,200}$"));
    return padrao.match(token).hasMatch();
}

bool IaService::modeloValido(const QString &modelo)
{
    static const QRegularExpression padrao(QStringLiteral("^@cf/[a-z0-9][a-z0-9._\\-]{0,60}/[a-z0-9][a-z0-9._\\-]{0,80}$"));
    return padrao.match(modelo).hasMatch() && !modelo.contains(QLatin1String(".."));
}

QUrl IaService::urlDaApi(const QString &accountId, const QString &modelo)
{
    if (!accountIdValido(accountId) || !modeloValido(modelo))
        return QUrl();
    return QUrl(QStringLiteral("%1/accounts/%2/ai/run/%3").arg(baseAtual(), accountId, modelo));
}

QByteArray IaService::montarCorpo(const QList<MensagemIa> &mensagens, int maxTokens, bool fluxo, double temperatura)
{
    QJsonArray lista;
    for (const MensagemIa &m : mensagens) {
        const QString papel = (m.papel == QLatin1String("system") || m.papel == QLatin1String("assistant")) ? m.papel : QStringLiteral("user");
        lista.append(QJsonObject{{QStringLiteral("role"), papel}, {QStringLiteral("content"), m.conteudo}});
    }
    QJsonObject corpo;
    corpo.insert(QStringLiteral("messages"), lista);
    corpo.insert(QStringLiteral("max_tokens"), qBound(16, maxTokens, 4096));
    corpo.insert(QStringLiteral("temperature"), qBound(0.0, temperatura, 2.0));
    if (fluxo)
        corpo.insert(QStringLiteral("stream"), true);
    return QJsonDocument(corpo).toJson(QJsonDocument::Compact);
}

std::optional<QString> IaService::trechoDaLinhaSse(const QByteArray &linha)
{
    QByteArray l = linha.trimmed();
    if (!l.startsWith("data:"))
        return std::nullopt;
    l = l.mid(5).trimmed();
    if (l.isEmpty() || l == "[DONE]")
        return std::nullopt;
    const QJsonDocument doc = QJsonDocument::fromJson(l);
    if (!doc.isObject())
        return QString();
    return textoDoObjeto(doc.object());
}

QString IaService::textoDaResposta(const QByteArray &corpoJson)
{
    const QJsonDocument doc = QJsonDocument::fromJson(corpoJson);
    return doc.isObject() ? textoDoObjeto(doc.object()) : QString();
}

QString IaService::mensagemDeErro(int status, const QByteArray &corpo, QNetworkReply::NetworkError erroDeRede)
{
    // Mensagem do próprio Cloudflare, quando houver (texto simples e curto; nunca o token).
    QString doServidor;
    const QJsonDocument doc = QJsonDocument::fromJson(corpo);
    if (doc.isObject()) {
        const QJsonArray erros = doc.object().value(QLatin1String("errors")).toArray();
        if (!erros.isEmpty())
            doServidor = textoCurto(erros.first().toObject().value(QLatin1String("message")).toString());
    }
    auto comDetalhe = [&](const QString &base) {
        return doServidor.isEmpty() ? base : QStringLiteral("%1\n\nDetalhe do serviço: %2").arg(base, doServidor);
    };

    switch (status) {
    case 400:
        return comDetalhe(QStringLiteral("O serviço de IA recusou o pedido. Tente um texto menor ou outro modelo."));
    case 401:
    case 403:
        return comDetalhe(QStringLiteral("O token foi recusado. Confira se ele está certo e se tem a permissão \"Workers AI\" "
                                         "(leitura e edição). Em Configurações > Assistente de IA há um link para o passo a passo."));
    case 404:
        return comDetalhe(QStringLiteral("Não encontrei esse modelo ou esse Account ID. Confira os dois nas Configurações."));
    case 408:
    case 504:
        return QStringLiteral("O serviço de IA demorou demais para responder. Tente de novo.");
    case 429:
        return comDetalhe(QStringLiteral("O limite gratuito de hoje (10.000 neurons) acabou ou há pedidos demais. "
                                         "A cota zera à meia-noite UTC (21h em Brasília). Modelos maiores gastam a cota mais rápido."));
    default:
        break;
    }
    if (status >= 500)
        return comDetalhe(QStringLiteral("O serviço de IA está com problemas agora. Tente de novo em instantes."));

    switch (erroDeRede) {
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
        return QStringLiteral("Sem conexão com a internet (ou o serviço está inacessível). A IA precisa de internet; o resto do programa continua funcionando.");
    case QNetworkReply::TimeoutError:
    case QNetworkReply::OperationCanceledError:
        return QStringLiteral("O serviço de IA demorou demais para responder. Tente de novo.");
    case QNetworkReply::SslHandshakeFailedError:
        return QStringLiteral("Falha na conexão segura (HTTPS) com o serviço de IA. Confira a data e a hora do computador.");
    default:
        break;
    }
    return comDetalhe(QStringLiteral("Não foi possível falar com o serviço de IA (código %1).").arg(status));
}

void IaService::usarServidorDeTeste(const QString &urlBase)
{
    const QUrl url(urlBase);
    // Só loopback e http: um teste nunca consegue apontar o token para a internet.
    if (url.isValid() && url.scheme() == QLatin1String("http") && url.host() == QLatin1String("127.0.0.1"))
        g_servidorDeTeste = urlBase;
    else
        g_servidorDeTeste.clear();
}

void IaService::enviar(const ConfigIa &config, const QList<MensagemIa> &mensagens, int maxTokens)
{
    if (ocupado()) {
        ResultadoIa r;
        r.erro = QStringLiteral("Já há um pedido em andamento.");
        emit concluido(r);
        return;
    }
    const QUrl url = urlDaApi(config.accountId, config.modelo);
    if (!url.isValid() || !tokenValido(config.token)) {
        ResultadoIa r;
        r.erro = QStringLiteral("A IA ainda não está configurada (ou os dados estão em formato inválido). "
                                "Abra Configurações > Assistente de IA.");
        emit concluido(r);
        return;
    }

    QNetworkRequest pedido(url);
    pedido.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    pedido.setRawHeader("Authorization", "Bearer " + config.token.toLatin1());
    pedido.setRawHeader("Accept", "text/event-stream, application/json");
    pedido.setRawHeader("User-Agent", ("Caderno+/" + versaoDoApp()).toLatin1());
    pedido.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);  // o token não segue redirecionamento
    pedido.setTransferTimeout(kTempoLimiteMs);

    m_pendente.clear();
    m_corpoBruto.clear();
    m_texto.clear();
    m_cancelando = false;
    m_estourou = false;

    m_resposta = m_rede->post(pedido, montarCorpo(mensagens, maxTokens, /*fluxo=*/true));
    connect(m_resposta, &QNetworkReply::readyRead, this, &IaService::lerDados);
    connect(m_resposta, &QNetworkReply::finished, this, &IaService::terminar);
}

void IaService::cancelar()
{
    if (!m_resposta)
        return;
    m_cancelando = true;
    m_resposta->abort();
}

void IaService::lerDados()
{
    if (m_resposta)
        acumular(m_resposta->readAll());
}

void IaService::acumular(const QByteArray &novo)
{
    if (!m_resposta || novo.isEmpty())
        return;
    if (m_corpoBruto.size() < kTamanhoMaximoDoCorpoBruto)
        m_corpoBruto += novo.left(kTamanhoMaximoDoCorpoBruto - m_corpoBruto.size());

    // Só processa como SSE se for mesmo SSE e sem erro HTTP; o resto fica no corpo bruto.
    const int status = m_resposta->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString tipo = m_resposta->header(QNetworkRequest::ContentTypeHeader).toString();
    if (status != 200 || !tipo.contains(QLatin1String("event-stream"), Qt::CaseInsensitive))
        return;

    m_pendente += novo;
    int fim;
    while ((fim = m_pendente.indexOf('\n')) >= 0) {
        const QByteArray linha = m_pendente.left(fim);
        m_pendente.remove(0, fim + 1);
        const auto pedaco = trechoDaLinhaSse(linha);
        if (pedaco && !pedaco->isEmpty()) {
            m_texto += *pedaco;
            emit trecho(*pedaco);
            if (m_texto.size() > kMaximoDeCaracteres && !m_estourou) {
                m_estourou = true;  // resposta absurdamente longa: corta aqui
                m_resposta->abort();
                return;
            }
        }
    }
}

void IaService::terminar()
{
    if (!m_resposta)
        return;
    QNetworkReply *resposta = m_resposta;
    const int status = resposta->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError erroDeRede = resposta->error();
    acumular(resposta->readAll());  // o que ainda não passou pelo readyRead

    ResultadoIa r;
    r.statusHttp = status;

    if (m_cancelando && !m_estourou) {
        r.cancelado = true;
        r.texto = m_texto;
        finalizar(r);
        return;
    }

    // Última linha do fluxo, se o servidor não terminou com quebra de linha.
    if (!m_pendente.isEmpty() && status == 200) {
        const auto pedaco = trechoDaLinhaSse(m_pendente);
        m_pendente.clear();
        if (pedaco && !pedaco->isEmpty()) {
            m_texto += *pedaco;
            emit trecho(*pedaco);
        }
    }

    if (m_estourou) {
        r.ok = true;
        r.texto = m_texto;
        finalizar(r);
        return;
    }
    if (status != 200 || (erroDeRede != QNetworkReply::NoError && m_texto.isEmpty())) {
        r.erro = mensagemDeErro(status, m_corpoBruto, erroDeRede);
        finalizar(r);
        return;
    }

    // Resposta inteira em JSON (o serviço não usou fluxo): lê o texto de uma vez.
    if (m_texto.isEmpty()) {
        const QString inteiro = textoDaResposta(m_corpoBruto);
        if (!inteiro.isEmpty()) {
            m_texto = inteiro;
            emit trecho(inteiro);
        }
    }
    if (m_texto.trimmed().isEmpty()) {
        r.erro = QStringLiteral("A IA não devolveu nenhum texto. Tente de novo, com outro pedido ou outro modelo.");
        finalizar(r);
        return;
    }
    r.ok = true;
    r.texto = m_texto;
    finalizar(r);
}

void IaService::finalizar(const ResultadoIa &resultado)
{
    if (m_resposta) {
        m_resposta->disconnect(this);
        m_resposta->deleteLater();
        m_resposta.clear();
    }
    emit concluido(resultado);
}

// ============================================================================
// Configuração salva
// ============================================================================

namespace IaConfig {

namespace {
const char *kChaveConta = "ia/accountId";
const char *kChaveModelo = "ia/modelo";
const char *kChaveToken = "ia/tokenProtegido";
const char *kChaveConsentimento = "ia/consentimento";
}  // namespace

ConfigIa carregar()
{
    QSettings s;
    ConfigIa c;
    c.accountId = s.value(QLatin1String(kChaveConta)).toString().trimmed();
    c.modelo = s.value(QLatin1String(kChaveModelo), IaService::modeloPadrao()).toString().trimmed();
    if (!IaService::modeloValido(c.modelo))
        c.modelo = IaService::modeloPadrao();
    c.token = SegredoService::revelar(s.value(QLatin1String(kChaveToken)).toString());
    return c;
}

bool salvar(const ConfigIa &config)
{
    QSettings s;
    s.setValue(QLatin1String(kChaveConta), config.accountId.trimmed());
    s.setValue(QLatin1String(kChaveModelo), IaService::modeloValido(config.modelo) ? config.modelo : IaService::modeloPadrao());
    if (config.token.isEmpty())
        return true;  // mantém o token já salvo
    const QString protegido = SegredoService::proteger(config.token);
    if (protegido.isEmpty())
        return false;
    s.setValue(QLatin1String(kChaveToken), protegido);
    return true;
}

bool temTokenSalvo()
{
    return !QSettings().value(QLatin1String(kChaveToken)).toString().isEmpty();
}

void apagarToken()
{
    QSettings().remove(QLatin1String(kChaveToken));
}

bool consentimentoDado()
{
    return QSettings().value(QLatin1String(kChaveConsentimento), false).toBool();
}

void definirConsentimento(bool dado)
{
    QSettings().setValue(QLatin1String(kChaveConsentimento), dado);
}

}  // namespace IaConfig
