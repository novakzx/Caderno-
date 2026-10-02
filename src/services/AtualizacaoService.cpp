#include "services/AtualizacaoService.h"

#include "core/BuildInfo.h"
#include "core/VersaoUtil.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {

QString g_servidorDeTeste;  // vazio = produção

constexpr qint64 kTamanhoMaximo = 512 * 1024;
constexpr int kTempoLimiteMs = 15 * 1000;

}  // namespace

AtualizacaoService::AtualizacaoService(QObject *parent) : QObject(parent), m_rede(new QNetworkAccessManager(this)) {}

QString AtualizacaoService::paginaDeReleases()
{
    return QStringLiteral("https://github.com/%1/releases").arg(QLatin1String(kRepositorio));
}

bool AtualizacaoService::urlConfiavel(const QString &texto)
{
    const QUrl url(texto, QUrl::StrictMode);
    if (!url.isValid() || url.scheme() != QLatin1String("https") || url.host() != QLatin1String("github.com"))
        return false;
    if (!url.userInfo().isEmpty() || url.port() != -1 || url.hasQuery() || url.hasFragment())
        return false;
    // Sem ".." nem texto codificado no caminho: nada de fugir da pasta das Releases ("releases/../../outro").
    if (url.path().contains(QLatin1String("..")) || url.path().contains(QLatin1Char('%')) || url.path().contains(QLatin1Char('\\')))
        return false;
    // Só a página de Releases do nosso repositório (ou uma etiqueta dela).
    const QString prefixo = QStringLiteral("/%1/releases").arg(QLatin1String(kRepositorio));
    const QString caminho = url.path();
    return caminho == prefixo || caminho.startsWith(prefixo + QLatin1Char('/'));
}

ResultadoAtualizacao AtualizacaoService::interpretar(const QByteArray &corpoJson, const QString &versaoAtual)
{
    ResultadoAtualizacao r;
    const QJsonDocument doc = QJsonDocument::fromJson(corpoJson);
    if (!doc.isObject()) {
        r.erro = QStringLiteral("Resposta inesperada do GitHub.");
        return r;
    }
    const QJsonObject o = doc.object();
    QString etiqueta = o.value(QLatin1String("tag_name")).toString().trimmed();
    if (!VersaoUtil::analisar(etiqueta.toStdString())) {
        r.erro = QStringLiteral("Não consegui ler o número da versão mais recente.");
        return r;
    }
    if (etiqueta.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        etiqueta.remove(0, 1);
    r.ok = true;
    r.versao = etiqueta;
    r.temNova = VersaoUtil::ehMaisNova(etiqueta.toStdString(), versaoAtual.toStdString());

    const QString url = o.value(QLatin1String("html_url")).toString();
    r.url = urlConfiavel(url) ? url : paginaDeReleases();  // nunca confia num endereço vindo de fora
    return r;
}

void AtualizacaoService::usarServidorDeTeste(const QString &urlBase)
{
    const QUrl url(urlBase);
    if (url.isValid() && url.scheme() == QLatin1String("http") && url.host() == QLatin1String("127.0.0.1"))
        g_servidorDeTeste = urlBase;
    else
        g_servidorDeTeste.clear();
}

void AtualizacaoService::verificar(const QString &versaoAtual)
{
    if (m_resposta)
        return;  // já há uma consulta em andamento
    const QString endereco = g_servidorDeTeste.isEmpty()
                                 ? QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(QLatin1String(kRepositorio))
                                 : g_servidorDeTeste;
    QNetworkRequest pedido{QUrl(endereco)};
    pedido.setRawHeader("Accept", "application/vnd.github+json");
    pedido.setRawHeader("User-Agent", ("Caderno+/" + versaoDoApp()).toLatin1());
    pedido.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    pedido.setTransferTimeout(kTempoLimiteMs);

    m_resposta = m_rede->get(pedido);
    QNetworkReply *resposta = m_resposta;
    connect(resposta, &QNetworkReply::finished, this, [this, resposta, versaoAtual] {
        ResultadoAtualizacao r;
        const int status = resposta->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray corpo = resposta->read(kTamanhoMaximo);
        if (status == 404) {
            r.erro = QStringLiteral("Ainda não há nenhuma versão publicada.");
        } else if (status != 200 || resposta->error() != QNetworkReply::NoError) {
            r.erro = QStringLiteral("Não foi possível consultar as atualizações (sem internet ou serviço indisponível).");
        } else {
            r = interpretar(corpo, versaoAtual);
        }
        resposta->deleteLater();
        m_resposta.clear();
        emit concluido(r);
    });
}
