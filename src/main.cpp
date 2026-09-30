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
#include "services/BackupService.h"
#include "ui/MainWindow.h"
#include "ui/ThemeManager.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Nome/organização definem a pasta de dados e as chaves do QSettings.
    QApplication::setApplicationName(QStringLiteral("ProfOrganizer"));
    QApplication::setOrganizationName(QStringLiteral("ProfOrganizer"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    ThemeManager::carregarSalvo();

    // Restauração de backup pendente (pedida na sessão anterior): precisa acontecer
    // ANTES de abrir o banco, pois troca o arquivo do banco.
    const QString caminhoBanco = DatabaseManager::caminhoPadrao();
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
                              QStringLiteral("Não foi possível abrir o banco de dados:\n\n%1")
                                  .arg(banco.ultimoErro()));
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

    MainWindow janela(repos);
    janela.show();

    return app.exec();
}
