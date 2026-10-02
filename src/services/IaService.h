#pragma once

#include <QByteArray>
#include <QList>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <optional>

class QNetworkAccessManager;

// Dados de acesso ao Cloudflare Workers AI (gratuito até 10.000 "neurons" por dia).
// Cada professor usa a PRÓPRIA conta e a PRÓPRIA chave: o programa não traz chave embutida.
struct ConfigIa {
    QString accountId;  // 32 caracteres hexadecimais (painel do Cloudflare)
    QString token;      // token de API com permissão "Workers AI"
    QString modelo;     // ex.: "@cf/meta/llama-3.1-8b-instruct-fp8"

    bool completa() const;
};

struct MensagemIa {
    QString papel;      // "system", "user" ou "assistant"
    QString conteudo;
};

struct ResultadoIa {
    bool ok = false;
    bool cancelado = false;
    QString texto;      // texto completo (quando ok)
    QString erro;       // mensagem para a pessoa (quando !ok e !cancelado)
    int statusHttp = 0;
};

// Cliente do Cloudflare Workers AI (REST, POST .../ai/run/<modelo>) com resposta em fluxo (SSE):
// o texto chega aos poucos pelo sinal `trecho`, e `concluido` fecha a chamada (sempre exatamente uma vez).
//
// Segurança:
//  - só HTTPS para api.cloudflare.com (a única exceção é o servidor de teste local, em 127.0.0.1);
//  - o token só vai no cabeçalho Authorization, nunca na URL nem em mensagens de erro ou logs;
//  - token, Account ID e modelo são validados antes de montar a requisição (nada de quebra de linha em
//    cabeçalho, nem "../" no caminho);
//  - redirecionamentos NÃO são seguidos (o token não pode seguir para outro endereço);
//  - tempo limite de transferência e teto de tamanho da resposta.
class IaService : public QObject {
    Q_OBJECT
public:
    explicit IaService(QObject *parent = nullptr);
    ~IaService() override;

    static QString modeloPadrao();
    static QStringList modelosSugeridos();

    // --- Validação e montagem (estáticas, testáveis sem rede) ---
    static bool accountIdValido(const QString &id);
    static bool tokenValido(const QString &token);
    static bool modeloValido(const QString &modelo);
    static QUrl urlDaApi(const QString &accountId, const QString &modelo);
    static QByteArray montarCorpo(const QList<MensagemIa> &mensagens, int maxTokens, bool fluxo, double temperatura = 0.6);
    // Linha de um fluxo SSE ("data: {...}"): o trecho de texto; "" se o evento não tem texto; nullopt para
    // "[DONE]" e para linhas que não são dados.
    static std::optional<QString> trechoDaLinhaSse(const QByteArray &linha);
    // Corpo JSON completo (sem fluxo): result.response (ou choices[0].message.content).
    static QString textoDaResposta(const QByteArray &corpoJson);
    static QString mensagemDeErro(int statusHttp, const QByteArray &corpo, QNetworkReply::NetworkError erroDeRede);

    // Só para testes: aponta o serviço para um servidor local (aceita apenas http://127.0.0.1:porta/...).
    static void usarServidorDeTeste(const QString &urlBase);

    // --- Uso ---
    void enviar(const ConfigIa &config, const QList<MensagemIa> &mensagens, int maxTokens = 1024);
    void cancelar();
    bool ocupado() const { return !m_resposta.isNull(); }

signals:
    void trecho(const QString &texto);
    void concluido(const ResultadoIa &resultado);

private:
    void lerDados();
    void acumular(const QByteArray &novo);
    void terminar();
    void finalizar(const ResultadoIa &resultado);

    QNetworkAccessManager *m_rede = nullptr;
    QPointer<QNetworkReply> m_resposta;
    QByteArray m_pendente;   // linhas SSE incompletas
    QByteArray m_corpoBruto; // para o caso de a resposta vir inteira em JSON (sem fluxo) ou de erro
    QString m_texto;
    bool m_cancelando = false;
    bool m_estourou = false;
};

// Configuração salva (Account ID e modelo em QSettings; o token protegido pela DPAPI do Windows).
namespace IaConfig {
ConfigIa carregar();
// `token` vazio mantém o token já salvo. Devolve false se o token não pôde ser protegido (fora do Windows).
bool salvar(const ConfigIa &config);
bool temTokenSalvo();
void apagarToken();
bool consentimentoDado();
void definirConsentimento(bool dado);
}  // namespace IaConfig
