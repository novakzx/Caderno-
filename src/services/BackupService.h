#pragma once

#include <QDateTime>
#include <QFileInfo>
#include <QList>
#include <QString>

// Backup do banco de dados (arquivo único SQLite).
//
// Como funciona:
//  - a cópia é feita com "VACUUM INTO", que gera um arquivo consistente mesmo com
//    o banco aberto (copiar o .db "na mão" poderia pegar dados pela metade);
//  - backups automáticos ficam em <pasta de dados>/backups, com data e hora no
//    nome, e só os mais recentes são mantidos;
//  - restaurar NÃO sobrescreve o banco em uso: o arquivo escolhido é validado e
//    deixado "pendente"; na próxima abertura do programa ele entra no lugar do
//    banco atual (que é guardado de lado, nunca apagado).
//
// Atenção: o backup guarda os DADOS (turmas, notas, anotações...) e os caminhos
// dos anexos, mas não os arquivos anexados em si.
namespace BackupService {

constexpr int kMaximoDeBackupsAutomaticos = 10;
constexpr int kIntervaloPadraoHoras = 24;

QString pastaDeBackups();

// Cria uma cópia consistente do banco em `destino` (sobrescreve se já existir).
bool criarBackup(const QString &destino, QString *erro);

// Faz um backup automático se o último tem mais de `intervaloHoras` (ou se não
// há nenhum) e apaga os excedentes. Retorna true se criou um backup novo.
bool backupAutomaticoSeNecessario(int intervaloHoras, QString *erro);

// Backups automáticos existentes, do mais recente para o mais antigo.
QList<QFileInfo> listarBackups();
QDateTime ultimoBackup();

// Verifica se o arquivo é um banco do programa, íntegro e de uma versão que esta
// versão do programa entende.
bool validarArquivo(const QString &arquivo, QString *erro);

// Deixa `arquivo` pronto para substituir o banco na próxima abertura do programa.
bool agendarRestauracao(const QString &arquivo, const QString &caminhoBanco, QString *erro);
bool restauracaoPendente(const QString &caminhoBanco);

// Chamar no início do programa, ANTES de abrir o banco: se houver restauração
// pendente, troca os arquivos. Devolve true se restaurou.
bool aplicarRestauracaoPendente(const QString &caminhoBanco, QString *erro);

}  // namespace BackupService
