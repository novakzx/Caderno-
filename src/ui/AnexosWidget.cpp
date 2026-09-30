#include "ui/AnexosWidget.h"

#include "core/AnexoUtil.h"
#include "database/AnexoRepository.h"

#include <QBrush>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

bool abrirAnexo(QWidget *parent, const QString &caminho)
{
    if (ehArquivoExecutavel(caminho)) {
        QMessageBox::warning(parent, QStringLiteral("Arquivo não aberto"),
                             QStringLiteral("Por segurança, o programa não abre executáveis nem scripts.\n\n%1").arg(caminho));
        return false;
    }
    if (!QFileInfo::exists(caminho)) {
        QMessageBox::warning(parent, QStringLiteral("Arquivo não encontrado"),
                             QStringLiteral("O arquivo não está mais neste local (foi movido, renomeado ou apagado):\n\n%1").arg(caminho));
        return false;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(caminho))) {
        QMessageBox::warning(parent, QStringLiteral("Não foi possível abrir"),
                             QStringLiteral("O sistema não encontrou um programa para abrir este arquivo:\n\n%1").arg(caminho));
        return false;
    }
    return true;
}

AnexosWidget::AnexosWidget(AnexoRepository &anexos, QWidget *parent) : QWidget(parent), m_anexos(anexos)
{
    auto *raiz = new QVBoxLayout(this);
    raiz->setContentsMargins(0, 0, 0, 0);
    raiz->setSpacing(8);

    m_lista = new QListWidget;
    m_lista->setAlternatingRowColors(true);
    raiz->addWidget(m_lista, 1);

    auto *botoes = new QHBoxLayout;
    m_btnAdicionar = new QPushButton(QStringLiteral("+ Adicionar arquivo…"));
    m_btnAdicionar->setObjectName(QStringLiteral("primary"));
    m_btnAbrir = new QPushButton(QStringLiteral("Abrir"));
    m_btnRemover = new QPushButton(QStringLiteral("Remover"));
    m_btnRemover->setObjectName(QStringLiteral("danger"));
    botoes->addWidget(m_btnAdicionar);
    botoes->addStretch(1);
    botoes->addWidget(m_btnAbrir);
    botoes->addWidget(m_btnRemover);
    raiz->addLayout(botoes);

    connect(m_btnAdicionar, &QPushButton::clicked, this, &AnexosWidget::adicionar);
    connect(m_btnAbrir, &QPushButton::clicked, this, &AnexosWidget::abrirSelecionado);
    connect(m_btnRemover, &QPushButton::clicked, this, &AnexosWidget::removerSelecionado);
    connect(m_lista, &QListWidget::itemDoubleClicked, this, [this] { abrirSelecionado(); });
    connect(m_lista, &QListWidget::itemSelectionChanged, this, &AnexosWidget::atualizarBotoes);

    atualizarBotoes();
}

void AnexosWidget::definirContexto(int turmaId, int aulaId)
{
    m_turmaId = turmaId;
    m_aulaId = aulaId;
    recarregar();
}

void AnexosWidget::recarregar()
{
    m_lista->clear();
    if (m_turmaId > 0) {
        const QList<Anexo> anexos = m_aulaId > 0 ? m_anexos.listarPorAula(m_aulaId) : m_anexos.listarPorTurma(m_turmaId);
        for (const Anexo &a : anexos) {
            QString detalhe = a.tipo.isEmpty() ? QStringLiteral("arquivo") : a.tipo.toUpper();
            if (m_aulaId == 0 && !a.aulaTema.isEmpty())
                detalhe += QStringLiteral(" · aula: %1").arg(a.aulaTema);

            const bool existe = QFileInfo::exists(a.caminho);
            auto *item = new QListWidgetItem(QStringLiteral("📎 %1\n%2%3")
                                                 .arg(a.nome, detalhe,
                                                      existe ? QString() : QStringLiteral(" · ARQUIVO NÃO ENCONTRADO")));
            item->setData(Qt::UserRole, a.id);
            item->setData(Qt::UserRole + 1, a.caminho);
            item->setToolTip(a.caminho);
            if (!existe)
                item->setForeground(QBrush(QColor(QStringLiteral("#D64545"))));
            m_lista->addItem(item);
        }
    }
    atualizarBotoes();
}

int AnexosWidget::anexoSelecionadoId() const
{
    const auto *item = m_lista->currentItem();
    return item ? item->data(Qt::UserRole).toInt() : 0;
}

void AnexosWidget::atualizarBotoes()
{
    const bool temContexto = m_turmaId > 0;
    const bool temItem = anexoSelecionadoId() != 0;
    m_btnAdicionar->setEnabled(temContexto);
    m_btnAbrir->setEnabled(temItem);
    m_btnRemover->setEnabled(temItem);
}

void AnexosWidget::adicionar()
{
    const QStringList arquivos = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Anexar arquivos"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QStringLiteral("Apresentações, documentos e planilhas (*.pptx *.ppt *.odp *.key *.pdf *.docx *.doc *.odt "
                       "*.xlsx *.xls *.ods *.txt);;Todos os arquivos (*)"));
    if (arquivos.isEmpty())
        return;

    int recusados = 0;
    for (const QString &caminho : arquivos) {
        if (ehArquivoExecutavel(caminho)) {
            ++recusados;  // executáveis não são aceitos como anexo
            continue;
        }
        const QFileInfo info(caminho);
        Anexo a;
        a.turmaId = m_turmaId;
        a.aulaId = m_aulaId;
        a.nome = info.fileName();
        a.caminho = info.absoluteFilePath();
        a.tipo = info.suffix().toLower();
        if (m_anexos.inserir(a) == 0) {
            QMessageBox::critical(this, QStringLiteral("Erro"), m_anexos.ultimoErro());
            break;
        }
    }
    if (recusados > 0)
        QMessageBox::information(this, QStringLiteral("Arquivos recusados"),
                                 QStringLiteral("%1 arquivo(s) executável(is) ou script(s) não foram anexados.").arg(recusados));
    recarregar();
}

void AnexosWidget::abrirSelecionado()
{
    if (const auto *item = m_lista->currentItem())
        abrirAnexo(this, item->data(Qt::UserRole + 1).toString());
}

void AnexosWidget::removerSelecionado()
{
    const int id = anexoSelecionadoId();
    const auto anexo = m_anexos.buscar(id);
    if (!anexo)
        return;

    const auto resp = QMessageBox::question(
        this, QStringLiteral("Remover anexo"),
        QStringLiteral("Remover \"%1\" desta lista?\n\nO arquivo em si NÃO é apagado do computador.").arg(anexo->nome),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (resp != QMessageBox::Yes)
        return;

    if (!m_anexos.remover(id))
        QMessageBox::critical(this, QStringLiteral("Erro"), m_anexos.ultimoErro());
    recarregar();
}
