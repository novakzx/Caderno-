#include "database/AgendaRepository.h"
#include "database/AlunoRepository.h"
#include "database/AnexoRepository.h"
#include "database/AnotacaoRepository.h"
#include "database/AulaRepository.h"
#include "database/AvaliacaoRepository.h"
#include "database/BuscaRepository.h"
#include "database/DatabaseManager.h"
#include "database/EventoRepository.h"
#include "database/FrequenciaRepository.h"
#include "database/HorarioRepository.h"
#include "database/NotaRepository.h"
#include "database/Repositorios.h"
#include "database/TarefaRepository.h"
#include "database/TurmaRepository.h"
#include "core/BuildInfo.h"
#include "services/AutoTeste.h"
#include "services/BackupService.h"
#include "services/ContaService.h"
#include "ui/LoginDialog.h"
#include "ui/MainWindow.h"
#include "ui/ThemeManager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    // Modo autoteste (usado pelo GitHub Actions): sem janela e sem tocar nos dados
    // do usuário. Uso: ProfOrganizer.exe --selftest relatorio.txt
    if (argc >= 3 && QByteArray(argv[1]) == "--selftest") {
        QCoreApplication app(argc, argv);
        return AutoTeste::executar(QString::fromLocal8Bit(argv[2]));
    }

    QApplication app(argc, argv);

    // Nome/organização definem a pasta de dados e as chaves do QSettings.
    QApplication::setApplicationName(QStringLiteral("ProfOrganizer"));
    QApplication::setOrganizationName(QStringLiteral("ProfOrganizer"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    // Nome exibido e ícone (o nome interno "ProfOrganizer" acima não muda: é a pasta dos dados).
    QApplication::setApplicationDisplayName(QStringLiteral("Caderno+"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/icon-256.png")));

    // Fonte Figtree (embutida) e tema salvo; a fonte precisa estar registrada antes do tema.
    ThemeManager::carregarFontes();
    ThemeManager::carregarSalvo();

    // Contas locais (login). O cadastro fica em contas.db, na pasta de dados do programa.
    const QString pastaBase = QFileInfo(DatabaseManager::caminhoPadrao()).absolutePath();
    ContaService contas(pastaBase);
    if (!contas.disponivel()) {
        QMessageBox::critical(nullptr, QStringLiteral("Erro no cadastro de contas"),
                              QStringLiteral("Não foi possível abrir o cadastro de contas:\n\n%1\n\nVersão: %2\nPasta: %3")
                                  .arg(contas.erroDeAbertura(), identificacaoDoBuild(), QDir::toNativeSeparators(pastaBase)));
        return 1;
    }

    // Uma "sessão" por conta: login -> janela principal. "Sair da conta" volta ao login.
    for (;;) {
        // Se já existe um banco de antes das contas, a primeira conta criada vai ficar com ele.
        const bool haDadosAntigos = !contas.temContas() && QFileInfo::exists(DatabaseManager::caminhoPadrao());
        LoginDialog login(contas, haDadosAntigos);
        if (login.exec() != QDialog::Accepted)
            return 0;
        const Conta conta = login.conta();

        const QString caminhoBanco = contas.caminhoDosDados(conta);
        DatabaseManager::definirCaminhoAtual(caminhoBanco);

        // Restauração de backup pendente (pedida na sessão anterior): precisa acontecer
        // ANTES de abrir o banco, pois troca o arquivo do banco.
        if (BackupService::restauracaoPendente(caminhoBanco)) {
            QString erroRestauracao;
            if (BackupService::aplicarRestauracaoPendente(caminhoBanco, &erroRestauracao))
                QMessageBox::information(nullptr, QStringLiteral("Backup restaurado"),
                                         QStringLiteral("Os dados do backup foram restaurados.\n\n"
                                                        "Os dados anteriores foram guardados na pasta de dados do programa, "
                                                        "em um arquivo \"antes-da-restauracao\"."));
            else
                QMessageBox::warning(nullptr, QStringLiteral("Restauração não aplicada"),
                                     QStringLiteral("Não foi possível restaurar o backup. Seus dados atuais foram mantidos.\n\n%1")
                                         .arg(erroRestauracao));
        }

        // Abre o banco e aplica as migrações pendentes antes de criar qualquer tela.
        DatabaseManager banco;
        if (!banco.abrir(caminhoBanco)) {
            QMessageBox::critical(nullptr, QStringLiteral("Erro no banco de dados"),
                                  QStringLiteral("Não foi possível abrir o banco de dados:\n\n%1\n\n"
                                                 "Versão: %2\nArquivo: %3")
                                      .arg(banco.ultimoErro(), identificacaoDoBuild(),
                                           QDir::toNativeSeparators(caminhoBanco)));
            return 1;
        }

        // Os repositórios são criados aqui e injetados nas telas (widgets não
        // criam nem conhecem o banco).
        TurmaRepository turmas;
        AlunoRepository alunos;
        AvaliacaoRepository avaliacoes;
        NotaRepository notas;
        HorarioRepository horarios;
        TarefaRepository tarefas;
        AgendaRepository agenda;
        AulaRepository aulas;
        AnexoRepository anexos;
        AnotacaoRepository anotacoes;
        FrequenciaRepository frequencia;
        EventoRepository eventos;
        BuscaRepository busca;
        Repositorios repos{turmas, alunos, avaliacoes, notas, horarios, tarefas, agenda,
                           aulas, anexos, anotacoes, frequencia, eventos, busca};

        MainWindow janela(repos, conta.nome, conta.email);
        bool trocarDeConta = false;
        QObject::connect(&janela, &MainWindow::trocarContaSolicitado, [&trocarDeConta] { trocarDeConta = true; });
        janela.show();

        app.exec();
        if (!trocarDeConta)
            break;
        // (a janela, os repositórios e o banco são destruídos aqui, antes do próximo login)
    }
    return 0;
}
