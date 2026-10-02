#include "ui/ConfiguracoesDialog.h"

#include "core/BuildInfo.h"
#include "services/AtualizacaoService.h"
#include "services/IaService.h"
#include "services/Preferencias.h"
#include "services/SegredoService.h"
#include "ui/GerenteDeLembretes.h"
#include "ui/ThemeManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QLabel *textoMudo(const QString &texto)
{
    auto *l = new QLabel(texto);
    l->setObjectName(QStringLiteral("muted"));
    l->setWordWrap(true);
    l->setTextFormat(Qt::PlainText);
    return l;
}

QWidget *pagina(QVBoxLayout **layout)
{
    auto *w = new QWidget;
    *layout = new QVBoxLayout(w);
    (*layout)->setContentsMargins(4, 14, 4, 4);
    (*layout)->setSpacing(12);
    return w;
}

QString rotuloDoBloqueio(int minutos)
{
    return minutos == 0 ? QStringLiteral("Nunca") : QStringLiteral("Depois de %1 minutos parado").arg(minutos);
}

QString rotuloDaAntecedencia(int minutos)
{
    return minutos == 0 ? QStringLiteral("Não avisar das aulas") : QStringLiteral("%1 minutos antes").arg(minutos);
}

}  // namespace

ConfiguracoesDialog::ConfiguracoesDialog(GerenteDeLembretes &gerente, Aba inicial, QWidget *parent)
    : QDialog(parent), m_gerente(gerente), m_ia(new IaService(this))
{
    setWindowTitle(QStringLiteral("Configurações"));
    setMinimumSize(620, 520);

    m_abas = new QTabWidget;
    m_abas->addTab(criarAbaSeguranca(), QStringLiteral("Segurança"));
    m_abas->addTab(criarAbaLembretes(), QStringLiteral("Lembretes"));
    m_abas->addTab(criarAbaAtualizacoes(), QStringLiteral("Atualizações"));
    m_abas->addTab(criarAbaIa(), QStringLiteral("Assistente de IA"));
    m_abas->setCurrentIndex(static_cast<int>(inicial));

    auto *botoes = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    botoes->button(QDialogButtonBox::Save)->setText(QStringLiteral("Salvar"));
    botoes->button(QDialogButtonBox::Save)->setObjectName(QStringLiteral("primary"));
    botoes->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
    connect(botoes, &QDialogButtonBox::accepted, this, &ConfiguracoesDialog::salvar);
    connect(botoes, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(12);
    auto *titulo = new QLabel(QStringLiteral("Configurações"));
    titulo->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(titulo);
    layout->addWidget(m_abas, 1);
    layout->addWidget(botoes);
}

// ---------------------------------------------------------------------------

QWidget *ConfiguracoesDialog::criarAbaSeguranca()
{
    QVBoxLayout *v;
    QWidget *w = pagina(&v);
    v->addWidget(textoMudo(QStringLiteral("O bloqueio protege o computador compartilhado (sala dos professores): depois de um tempo sem "
                                          "você mexer no programa, ele volta para a tela de login. O que você estava editando é salvo antes.")));
    m_bloqueio = new QComboBox;
    for (int minutos : Preferencias::kBloqueiosPossiveis)
        m_bloqueio->addItem(rotuloDoBloqueio(minutos), minutos);
    m_bloqueio->setCurrentIndex(qMax(0, m_bloqueio->findData(Preferencias::bloqueioEmMinutos())));
    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Bloquear"), m_bloqueio);
    v->addLayout(form);
    v->addWidget(textoMudo(QStringLiteral("Dica: o Windows também pode bloquear a tela sozinho (Win + L bloqueia na hora).")));
    v->addStretch(1);
    return w;
}

QWidget *ConfiguracoesDialog::criarAbaLembretes()
{
    QVBoxLayout *v;
    QWidget *w = pagina(&v);
    const LembreteUtil::Config atual = GerenteDeLembretes::configuracao();

    v->addWidget(textoMudo(QStringLiteral("O Caderno+ avisa com uma notificação do Windows, ao lado do relógio. Os avisos aparecem "
                                          "enquanto o programa estiver aberto.")));
    m_lembretesAtivos = new QCheckBox(QStringLiteral("Ativar lembretes"));
    m_lembretesAtivos->setChecked(atual.ativos);
    m_antecedencia = new QComboBox;
    for (int minutos : LembreteUtil::kAntecedenciasPossiveis)
        m_antecedencia->addItem(rotuloDaAntecedencia(minutos), minutos);
    m_antecedencia->setCurrentIndex(qMax(0, m_antecedencia->findData(atual.antecedenciaAula)));
    m_prazos = new QCheckBox(QStringLiteral("Avisar de tarefas e provas de hoje e de amanhã (a partir das 8h)"));
    m_prazos->setChecked(atual.prazos);

    auto *form = new QFormLayout;
    form->addRow(QString(), m_lembretesAtivos);
    form->addRow(QStringLiteral("Aulas"), m_antecedencia);
    form->addRow(QStringLiteral("Prazos"), m_prazos);
    v->addLayout(form);

    auto *teste = new QPushButton(QStringLiteral("Enviar notificação de teste"));
    connect(teste, &QPushButton::clicked, this, [this] { m_gerente.notificarTeste(); });
    v->addWidget(teste, 0, Qt::AlignLeft);
    v->addWidget(textoMudo(QStringLiteral("Se a notificação de teste não aparecer, o \"Assistente de foco\" do Windows pode estar ligado "
                                          "(Configurações do Windows > Sistema > Foco).")));
    if (!m_gerente.bandejaDisponivel()) {
        auto *aviso = new QLabel(QStringLiteral("Este computador não oferece a área de notificação do Windows; os lembretes não podem ser mostrados."));
        aviso->setWordWrap(true);
        v->addWidget(aviso);
        teste->setEnabled(false);
        m_lembretesAtivos->setEnabled(false);
    }
    v->addStretch(1);
    return w;
}

QWidget *ConfiguracoesDialog::criarAbaAtualizacoes()
{
    QVBoxLayout *v;
    QWidget *w = pagina(&v);
    v->addWidget(textoMudo(QStringLiteral("Versão instalada: %1").arg(identificacaoDoBuild())));
    v->addWidget(textoMudo(QStringLiteral("O Caderno+ funciona sem internet. Se você quiser, ele pode consultar o GitHub ao abrir (no máximo uma vez por dia) "
                                          "para avisar quando houver uma versão nova. Nenhum dado seu é enviado; o GitHub só vê que houve uma consulta. "
                                          "O programa nunca baixa nem instala nada sozinho: quem baixa é você, pelo navegador.")));
    m_verificarAtualizacoes = new QCheckBox(QStringLiteral("Avisar quando houver versão nova"));
    m_verificarAtualizacoes->setChecked(Preferencias::verificarAtualizacoes() == Preferencias::Atualizacao::Sim);
    v->addWidget(m_verificarAtualizacoes);

    m_botaoVerificar = new QPushButton(QStringLiteral("Verificar agora"));
    connect(m_botaoVerificar, &QPushButton::clicked, this, &ConfiguracoesDialog::verificarAgora);
    v->addWidget(m_botaoVerificar, 0, Qt::AlignLeft);
    m_resultadoAtualizacao = new QLabel;
    m_resultadoAtualizacao->setWordWrap(true);
    m_resultadoAtualizacao->setTextFormat(Qt::PlainText);
    v->addWidget(m_resultadoAtualizacao);
    v->addStretch(1);
    return w;
}

QWidget *ConfiguracoesDialog::criarAbaIa()
{
    QVBoxLayout *v;
    QWidget *w = pagina(&v);
    v->addWidget(textoMudo(QStringLiteral("O Assistente usa o Cloudflare Workers AI, que tem um uso gratuito diário (10.000 \"neurons\"). "
                                          "Você usa a SUA conta gratuita do Cloudflare: crie uma conta, copie o Account ID e crie um token com a "
                                          "permissão \"Workers AI\". O passo a passo está em docs/GUIA-IA.md.")));

    const ConfigIa atual = IaConfig::carregar();
    m_iaConta = new QLineEdit(atual.accountId);
    m_iaConta->setMaxLength(32);
    m_iaConta->setPlaceholderText(QStringLiteral("32 letras e números (Account ID)"));
    m_iaToken = new QLineEdit;
    m_iaToken->setEchoMode(QLineEdit::Password);
    m_iaToken->setMaxLength(200);
    m_iaToken->setPlaceholderText(IaConfig::temTokenSalvo() ? QStringLiteral("deixe em branco para manter a chave salva")
                                                            : QStringLiteral("cole aqui o token de API"));
    m_iaEstadoDoToken = new QLabel;
    m_iaEstadoDoToken->setObjectName(QStringLiteral("muted"));
    m_iaEstadoDoToken->setWordWrap(true);
    m_iaModelo = new QComboBox;
    m_iaModelo->setEditable(true);
    m_iaModelo->addItems(IaService::modelosSugeridos());
    m_iaModelo->setCurrentText(atual.modelo);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Account ID"), m_iaConta);
    form->addRow(QStringLiteral("Token de API"), m_iaToken);
    form->addRow(QString(), m_iaEstadoDoToken);
    form->addRow(QStringLiteral("Modelo"), m_iaModelo);
    v->addLayout(form);

    if (SegredoService::disponivel()) {
        m_iaEstadoDoToken->setText(IaConfig::temTokenSalvo() ? QStringLiteral("Uma chave está salva, protegida pelo Windows (só este usuário neste computador consegue abri-la).")
                                                             : QStringLiteral("Nenhuma chave salva. Ao salvar, ela fica protegida pelo Windows."));
    } else {
        m_iaEstadoDoToken->setText(QStringLiteral("Este sistema não oferece proteção de segredos: a chave não pode ser salva."));
        m_iaToken->setEnabled(false);
    }

    auto *botoes = new QHBoxLayout;
    m_botaoTestarIa = new QPushButton(QStringLiteral("Testar conexão"));
    auto *apagar = new QPushButton(QStringLiteral("Apagar chave salva"));
    apagar->setObjectName(QStringLiteral("danger"));
    botoes->addWidget(m_botaoTestarIa);
    botoes->addWidget(apagar);
    botoes->addStretch(1);
    v->addLayout(botoes);
    m_iaResultado = new QLabel;
    m_iaResultado->setWordWrap(true);
    m_iaResultado->setTextFormat(Qt::PlainText);
    v->addWidget(m_iaResultado);

    v->addWidget(textoMudo(QStringLiteral("Privacidade: só vai para o Cloudflare o texto que você escreve no Assistente. O programa nunca envia "
                                          "dados dos seus alunos por conta própria. Evite digitar nomes de alunos.")));
    v->addStretch(1);

    connect(m_botaoTestarIa, &QPushButton::clicked, this, &ConfiguracoesDialog::testarIa);
    connect(apagar, &QPushButton::clicked, this, [this] {
        IaConfig::apagarToken();
        m_iaToken->clear();
        m_iaToken->setPlaceholderText(QStringLiteral("cole aqui o token de API"));
        m_iaEstadoDoToken->setText(QStringLiteral("Nenhuma chave salva."));
        m_iaResultado->setText(QStringLiteral("Chave apagada."));
        ThemeManager::definirEstado(m_iaResultado, ThemeManager::Estado::Neutro);
    });
    connect(m_ia, &IaService::concluido, this, [this](const ResultadoIa &r) {
        m_botaoTestarIa->setEnabled(true);
        if (r.ok) {
            m_iaResultado->setText(QStringLiteral("Conexão funcionando. A IA respondeu: \"%1\"").arg(r.texto.simplified().left(80)));
            ThemeManager::definirEstado(m_iaResultado, ThemeManager::Estado::Sucesso);
        } else {
            m_iaResultado->setText(r.erro);
            ThemeManager::definirEstado(m_iaResultado, ThemeManager::Estado::Erro);
        }
    });
    return w;
}

// ---------------------------------------------------------------------------

void ConfiguracoesDialog::verificarAgora()
{
    m_botaoVerificar->setEnabled(false);
    m_resultadoAtualizacao->setText(QStringLiteral("Consultando…"));
    ThemeManager::definirEstado(m_resultadoAtualizacao, ThemeManager::Estado::Neutro);
    auto *servico = new AtualizacaoService(this);
    connect(servico, &AtualizacaoService::concluido, this, [this, servico](const ResultadoAtualizacao &r) {
        m_botaoVerificar->setEnabled(true);
        if (!r.ok) {
            m_resultadoAtualizacao->setText(r.erro);
            ThemeManager::definirEstado(m_resultadoAtualizacao, ThemeManager::Estado::Erro);
        } else if (r.temNova) {
            m_resultadoAtualizacao->setText(QStringLiteral("Há uma versão nova: %1. Abrindo a página de download…").arg(r.versao));
            ThemeManager::definirEstado(m_resultadoAtualizacao, ThemeManager::Estado::Aviso);
            QDesktopServices::openUrl(QUrl(r.url));
        } else {
            m_resultadoAtualizacao->setText(QStringLiteral("Você está com a versão mais recente (%1).").arg(r.versao));
            ThemeManager::definirEstado(m_resultadoAtualizacao, ThemeManager::Estado::Sucesso);
        }
        servico->deleteLater();
    });
    servico->verificar(versaoDoApp());
}

void ConfiguracoesDialog::testarIa()
{
    ConfigIa c;
    c.accountId = m_iaConta->text().trimmed();
    c.token = m_iaToken->text().trimmed();
    c.modelo = m_iaModelo->currentText().trimmed();
    if (c.token.isEmpty())
        c.token = IaConfig::carregar().token;  // campo em branco = usa a chave salva
    if (!c.completa()) {
        m_iaResultado->setText(QStringLiteral("Preencha o Account ID (32 letras e números), o token e um modelo válido (ex.: @cf/meta/llama-3.1-8b-instruct-fp8)."));
        ThemeManager::definirEstado(m_iaResultado, ThemeManager::Estado::Erro);
        return;
    }
    m_botaoTestarIa->setEnabled(false);
    m_iaResultado->setText(QStringLiteral("Testando…"));
    ThemeManager::definirEstado(m_iaResultado, ThemeManager::Estado::Neutro);
    m_ia->enviar(c, {{QStringLiteral("user"), QStringLiteral("Responda apenas com a palavra: ok")}}, 16);
}

bool ConfiguracoesDialog::salvarIa()
{
    ConfigIa c;
    c.accountId = m_iaConta->text().trimmed();
    c.token = m_iaToken->text().trimmed();
    c.modelo = m_iaModelo->currentText().trimmed();

    if (!c.accountId.isEmpty() && !IaService::accountIdValido(c.accountId)) {
        m_abas->setCurrentIndex(static_cast<int>(Aba::Ia));
        QMessageBox::warning(this, QStringLiteral("Account ID inválido"),
                             QStringLiteral("O Account ID tem 32 letras e números (de 0 a 9 e de a até f). Copie-o de novo no painel do Cloudflare."));
        m_iaConta->setFocus();
        return false;
    }
    if (!c.token.isEmpty() && !IaService::tokenValido(c.token)) {
        m_abas->setCurrentIndex(static_cast<int>(Aba::Ia));
        QMessageBox::warning(this, QStringLiteral("Token inválido"),
                             QStringLiteral("O token só pode ter letras, números, \"-\" e \"_\" (sem espaços). Copie-o de novo, sem nada a mais."));
        m_iaToken->setFocus();
        return false;
    }
    if (!IaService::modeloValido(c.modelo)) {
        m_abas->setCurrentIndex(static_cast<int>(Aba::Ia));
        QMessageBox::warning(this, QStringLiteral("Modelo inválido"),
                             QStringLiteral("O modelo tem o formato @cf/empresa/nome-do-modelo (ex.: @cf/meta/llama-3.1-8b-instruct-fp8)."));
        return false;
    }
    if (!IaConfig::salvar(c)) {
        QMessageBox::warning(this, QStringLiteral("Chave não salva"),
                             QStringLiteral("Não foi possível proteger a chave neste sistema, então ela não foi salva."));
        return false;
    }
    return true;
}

void ConfiguracoesDialog::salvar()
{
    if (!salvarIa())
        return;

    Preferencias::definirBloqueioEmMinutos(m_bloqueio->currentData().toInt());

    LembreteUtil::Config lembretes;
    lembretes.ativos = m_lembretesAtivos->isChecked();
    lembretes.antecedenciaAula = m_antecedencia->currentData().toInt();
    lembretes.prazos = m_prazos->isChecked();
    GerenteDeLembretes::salvarConfiguracao(lembretes);
    m_gerente.configuracaoAlterada();

    Preferencias::definirVerificarAtualizacoes(m_verificarAtualizacoes->isChecked());
    accept();
}
