#include "ui/BackupDialog.h"
#include "ui/ThemeManager.h"

#include "database/DatabaseManager.h"
#include "services/BackupService.h"

#include <QApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QString tamanhoLegivel(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024);
    return QStringLiteral("%1 MB").arg(QString::number(bytes / 1024.0 / 1024.0, 'f', 1));
}

}  // namespace

BackupDialog::BackupDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Backup dos dados"));
    resize(620, 480);

    auto *explicacao = new QLabel(QStringLiteral(
        "O programa faz um backup automático a cada 24 horas (mantendo os %1 mais recentes). "
        "O backup guarda todos os seus dados, mas <b>não os arquivos anexados</b> (apresentações, PDFs…): "
        "esses continuam onde estão no computador.")
                                      .arg(BackupService::kMaximoDeBackupsAutomaticos));
    explicacao->setWordWrap(true);
    explicacao->setTextFormat(Qt::RichText);

    m_ultimo = new QLabel;
    m_ultimo->setObjectName(QStringLiteral("muted"));

    m_lista = new QListWidget;
    m_lista->setAlternatingRowColors(true);

    auto *btnAgora = new QPushButton(QStringLiteral("Fazer backup agora"));
    btnAgora->setObjectName(QStringLiteral("primary"));
    ThemeManager::iconeNoBotao(btnAgora, QStringLiteral("backup"), Tokens::Id::OnPrimary);
    auto *btnCopia = new QPushButton(QStringLiteral("Salvar uma cópia em…"));
    m_btnRestaurar = new QPushButton(QStringLiteral("Restaurar o selecionado…"));
    auto *btnArquivo = new QPushButton(QStringLiteral("Restaurar de um arquivo…"));
    auto *btnPasta = new QPushButton(QStringLiteral("Abrir pasta dos backups"));
    auto *btnFechar = new QPushButton(QStringLiteral("Fechar"));

    auto *linha1 = new QHBoxLayout;
    linha1->addWidget(btnAgora);
    linha1->addWidget(btnCopia);
    linha1->addWidget(btnPasta);
    linha1->addStretch(1);
    auto *linha2 = new QHBoxLayout;
    linha2->addWidget(m_btnRestaurar);
    linha2->addWidget(btnArquivo);
    linha2->addStretch(1);
    linha2->addWidget(btnFechar);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(10);
    layout->addWidget(explicacao);
    layout->addWidget(m_ultimo);
    layout->addWidget(m_lista, 1);
    layout->addLayout(linha1);
    layout->addLayout(linha2);

    connect(btnAgora, &QPushButton::clicked, this, &BackupDialog::fazerBackupAgora);
    connect(btnCopia, &QPushButton::clicked, this, &BackupDialog::salvarCopia);
    connect(m_btnRestaurar, &QPushButton::clicked, this, &BackupDialog::restaurarSelecionado);
    connect(btnArquivo, &QPushButton::clicked, this, &BackupDialog::restaurarDeArquivo);
    connect(btnPasta, &QPushButton::clicked, this, &BackupDialog::abrirPasta);
    connect(btnFechar, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_lista, &QListWidget::itemSelectionChanged, this, [this] { m_btnRestaurar->setEnabled(m_lista->currentItem()); });

    recarregar();
}

void BackupDialog::recarregar()
{
    m_lista->clear();
    for (const QFileInfo &f : BackupService::listarBackups()) {
        auto *item = new QListWidgetItem(QStringLiteral("%1   ·   %2")
                                             .arg(f.lastModified().toString(QStringLiteral("dd/MM/yyyy HH:mm")),
                                                  tamanhoLegivel(f.size())));
        item->setData(Qt::UserRole, f.absoluteFilePath());
        item->setToolTip(f.absoluteFilePath());
        m_lista->addItem(item);
    }
    const QDateTime ultimo = BackupService::ultimoBackup();
    m_ultimo->setText(ultimo.isValid() ? QStringLiteral("Último backup: %1").arg(ultimo.toString(QStringLiteral("dd/MM/yyyy HH:mm")))
                                       : QStringLiteral("Nenhum backup feito ainda."));
    m_btnRestaurar->setEnabled(false);
}

void BackupDialog::fazerBackupAgora()
{
    QString erro;
    // Intervalo 0: força um backup novo agora.
    if (!BackupService::backupAutomaticoSeNecessario(0, &erro)) {
        QMessageBox::critical(this, QStringLiteral("Erro no backup"),
                              erro.isEmpty() ? QStringLiteral("Não foi possível criar o backup.") : erro);
        return;
    }
    recarregar();
    QMessageBox::information(this, QStringLiteral("Backup concluído"), QStringLiteral("Backup criado com sucesso."));
}

void BackupDialog::salvarCopia()
{
    const QString pasta = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString destino = QFileDialog::getSaveFileName(
        this, QStringLiteral("Salvar cópia do banco de dados"),
        pasta + QStringLiteral("/professor-backup-%1.db").arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))),
        QStringLiteral("Banco de dados (*.db)"));
    if (destino.isEmpty())
        return;
    if (!destino.endsWith(QStringLiteral(".db"), Qt::CaseInsensitive))
        destino += QStringLiteral(".db");

    QString erro;
    if (!BackupService::criarBackup(destino, &erro)) {
        QMessageBox::critical(this, QStringLiteral("Erro no backup"), erro);
        return;
    }
    QMessageBox::information(this, QStringLiteral("Cópia salva"), QStringLiteral("Cópia salva em:\n%1").arg(destino));
}

void BackupDialog::restaurarSelecionado()
{
    if (const auto *item = m_lista->currentItem())
        agendarRestauracao(item->data(Qt::UserRole).toString());
}

void BackupDialog::restaurarDeArquivo()
{
    const QString arquivo = QFileDialog::getOpenFileName(
        this, QStringLiteral("Escolher backup para restaurar"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), QStringLiteral("Banco de dados (*.db);;Todos os arquivos (*)"));
    if (!arquivo.isEmpty())
        agendarRestauracao(arquivo);
}

void BackupDialog::agendarRestauracao(const QString &arquivo)
{
    const auto resp = QMessageBox::warning(
        this, QStringLiteral("Restaurar backup"),
        QStringLiteral("Restaurar este backup substitui TODOS os dados atuais pelos dados do backup.\n\n"
                       "Por segurança, os dados atuais não são apagados: ficam guardados ao lado, em um arquivo "
                       "\"antes-da-restauracao\", na pasta de dados do programa.\n\n"
                       "A restauração acontece quando o programa for aberto novamente. Continuar?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (resp != QMessageBox::Yes)
        return;

    QString erro;
    if (!BackupService::agendarRestauracao(arquivo, DatabaseManager::caminhoAtual(), &erro)) {
        QMessageBox::critical(this, QStringLiteral("Não foi possível restaurar"), erro);
        return;
    }

    const auto fechar = QMessageBox::question(
        this, QStringLiteral("Restauração agendada"),
        QStringLiteral("O backup foi validado e será aplicado na próxima abertura do programa.\n\nFechar o programa agora?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (fechar == QMessageBox::Yes)
        QApplication::quit();
}

void BackupDialog::abrirPasta()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(BackupService::pastaDeBackups()));
}
