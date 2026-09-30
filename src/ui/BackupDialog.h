#pragma once

#include <QDialog>

class QLabel;
class QListWidget;
class QPushButton;

// Janela de backup: lista os backups automáticos, cria novos, salva uma cópia
// em outro lugar (pen drive, nuvem) e restaura um backup.
class BackupDialog : public QDialog {
    Q_OBJECT
public:
    explicit BackupDialog(QWidget *parent = nullptr);

private:
    void recarregar();
    void fazerBackupAgora();
    void salvarCopia();
    void restaurarSelecionado();
    void restaurarDeArquivo();
    void agendarRestauracao(const QString &arquivo);
    void abrirPasta();

    QLabel *m_ultimo = nullptr;
    QListWidget *m_lista = nullptr;
    QPushButton *m_btnRestaurar = nullptr;
};
