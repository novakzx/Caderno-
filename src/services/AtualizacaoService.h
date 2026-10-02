#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

struct ResultadoAtualizacao {
    bool ok = false;         // a consulta funcionou (mesmo que não haja versão nova)
    bool temNova = false;
    QString versao;          // "1.2.0" (sem o "v")
    QString url;             // página da Release (sempre em github.com/<repositório>/releases)
    QString erro;
};

// Consulta no GitHub se há uma Release mais nova (GET api.github.com/repos/<repo>/releases/latest).
// Não envia nenhum dado do usuário, só o pedido em si (o GitHub vê o endereço IP, como em qualquer site).
// Nunca baixa nem executa nada: só informa a versão e a página, e quem baixa é a pessoa, no navegador.
class AtualizacaoService : public QObject {
    Q_OBJECT
public:
    static constexpr const char *kRepositorio = "novakzx/Caderno-";

    explicit AtualizacaoService(QObject *parent = nullptr);

    // Interpreta o JSON da API (testável sem rede). A URL só é aceita se for da página de Releases do repositório.
    static ResultadoAtualizacao interpretar(const QByteArray &corpoJson, const QString &versaoAtual);
    static QString paginaDeReleases();
    static bool urlConfiavel(const QString &url);

    // Só para testes: aponta para um servidor local (http://127.0.0.1:porta/...).
    static void usarServidorDeTeste(const QString &urlBase);

    void verificar(const QString &versaoAtual);

signals:
    void concluido(const ResultadoAtualizacao &resultado);

private:
    QNetworkAccessManager *m_rede = nullptr;
    QPointer<QNetworkReply> m_resposta;
};
