#include "services/AutoTeste.h"

#include "core/BuildInfo.h"
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
#include "database/Migrations.h"
#include "database/NotaRepository.h"
#include "database/Repositorios.h"
#include "database/SqlUtil.h"
#include "database/TarefaRepository.h"
#include "database/TurmaRepository.h"
#include "services/BackupService.h"
#include "services/DesempenhoService.h"
#include "services/RelatorioPdf.h"

#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

namespace AutoTeste {

namespace {

// O relatório é gravado linha a linha, com flush a cada uma: se o programa travar
// no meio, o que já foi verificado continua no arquivo e dá para ver onde parou.
struct Relatorio {
    QFile arquivo;
    int falhas = 0;

    explicit Relatorio(const QString &caminho)
    {
        arquivo.setFileName(caminho);
        arquivo.open(QIODevice::WriteOnly | QIODevice::Truncate);
    }

    void linha(const QString &t)
    {
        if (arquivo.isOpen()) {
            arquivo.write(t.toUtf8());
            arquivo.write("\n");
            arquivo.flush();
        }
    }
    void titulo(const QString &t)
    {
        linha(QString());
        linha(QStringLiteral("== %1 ==").arg(t));
    }
    void info(const QString &t) { linha(QStringLiteral("   ") + t); }
    void verificar(const QString &nome, bool condicao, const QString &detalhe = QString())
    {
        if (condicao) {
            linha(QStringLiteral("  OK      %1").arg(nome));
        } else {
            ++falhas;
            linha(QStringLiteral("  FALHOU  %1%2").arg(nome, detalhe.isEmpty() ? QString() : QStringLiteral(" -> ") + detalhe));
        }
    }
};

// ---------------------------------------------------------------------------
// Variantes de abertura do banco: servem para isolar o que provoca um erro
// ("cannot commit transaction - SQL statements in progress", por exemplo).
// Cada variante usa um arquivo e uma conexão próprios.
// ---------------------------------------------------------------------------
struct Variante {
    QString nome;
    bool chavesEstrangeiras = false;  // PRAGMA foreign_keys = ON antes
    bool wal = false;                 // PRAGMA journal_mode = WAL antes
    bool transacaoDoQt = false;       // db.transaction()/commit() em vez de BEGIN/COMMIT explícitos
    bool gravarVersao = true;         // PRAGMA user_version dentro da transação
    bool usarAplicar = false;         // Migrations::aplicar (todas as migrações) em vez de só a 1ª
};

QString rodarVariante(const Variante &v, const QString &pasta)
{
    const QString conexao = QStringLiteral("variante_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString resultado = QStringLiteral("OK");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conexao);
        db.setDatabaseName(pasta + QLatin1Char('/') + conexao + QStringLiteral(".db"));
        QString erro;
        if (!db.open()) {
            resultado = QStringLiteral("não abriu: ") + db.lastError().text();
        } else {
            if (v.chavesEstrangeiras)
                Migrations::executarComando(db, QStringLiteral("PRAGMA foreign_keys = ON"), &erro);
            if (v.wal)
                Migrations::executarComando(db, QStringLiteral("PRAGMA journal_mode = WAL"), &erro);

            if (v.usarAplicar) {
                if (!Migrations::aplicar(db, &erro))
                    resultado = erro;
            } else {
                const Migrations::Migracao &m = Migrations::todas().first();
                bool ok = v.transacaoDoQt ? db.transaction()
                                          : Migrations::executarComando(db, QStringLiteral("BEGIN IMMEDIATE"), &erro);
                if (!ok) {
                    resultado = QStringLiteral("BEGIN: ") + (erro.isEmpty() ? db.lastError().text() : erro);
                } else {
                    for (const QString &sql : m.comandos) {
                        if (!Migrations::executarComando(db, sql, &erro)) {
                            resultado = QStringLiteral("comando falhou: ") + erro;
                            ok = false;
                            break;
                        }
                    }
                    if (ok && v.gravarVersao)
                        ok = Migrations::executarComando(db, QStringLiteral("PRAGMA user_version = 1"), &erro);
                    if (ok) {
                        const bool confirmou = v.transacaoDoQt ? db.commit()
                                                               : Migrations::executarComando(db, QStringLiteral("COMMIT"), &erro);
                        if (!confirmou)
                            resultado = QStringLiteral("COMMIT: ") + (erro.isEmpty() ? db.lastError().text() : erro);
                    } else if (resultado == QLatin1String("OK")) {
                        resultado = QStringLiteral("falhou antes do commit: ") + erro;
                    }
                }
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(conexao);
    return resultado;
}

void rodarVariantes(Relatorio &r, const QString &pasta)
{
    r.titulo(QStringLiteral("Variantes de abertura do banco (diagnóstico)"));
    const QList<Variante> variantes = {
        {QStringLiteral("migração 1, BEGIN/COMMIT explícitos, sem PRAGMAs, sem user_version"), false, false, false, false, false},
        {QStringLiteral("migração 1 + user_version"), false, false, false, true, false},
        {QStringLiteral("migração 1 + user_version + foreign_keys=ON"), true, false, false, true, false},
        {QStringLiteral("migração 1 + user_version + foreign_keys=ON, transação do Qt"), true, false, true, true, false},
        {QStringLiteral("migração 1 + user_version, journal_mode=WAL antes"), false, true, false, true, false},
        {QStringLiteral("TODAS as migrações (Migrations::aplicar), sem PRAGMAs"), false, false, false, true, true},
        {QStringLiteral("TODAS as migrações + foreign_keys=ON"), true, false, false, true, true},
        {QStringLiteral("TODAS as migrações + foreign_keys=ON + WAL antes"), true, true, false, true, true},
    };
    for (const Variante &v : variantes) {
        const QString res = rodarVariante(v, pasta);
        r.verificar(v.nome, res == QLatin1String("OK"), res);
    }
}

// ---------------------------------------------------------------------------
// Teste de fumaça: exercita os repositórios no banco aberto pelo DatabaseManager.
// ---------------------------------------------------------------------------
void testeDeFumaca(Relatorio &r, const QString &pasta)
{
    // --- ligar(): texto nulo não pode virar NULL no SQLite ---
    r.titulo(QStringLiteral("Texto nulo x NULL (SqlUtil::ligar)"));
    {
        auto eNulo = [&](const QVariant &valor, bool *ok) {
            QSqlQuery q;
            q.prepare(QStringLiteral("SELECT :x IS NULL"));
            ligar(q, QStringLiteral(":x"), valor);
            *ok = q.exec() && q.next();
            const int resultado = *ok ? q.value(0).toInt() : -1;
            q.finish();
            return resultado;
        };
        bool ok = false;
        const QString vazioNaoNulo = QString::fromLatin1("");
        r.verificar(QStringLiteral("QString::fromLatin1(\"\") não é nulo no Qt"), !vazioNaoNulo.isNull());
        r.verificar(QStringLiteral("ligar(QString()) grava texto vazio (não NULL)"), eNulo(QVariant(QString()), &ok) == 0 && ok);
        r.verificar(QStringLiteral("ligar(QString(\"\")) grava texto vazio (não NULL)"), eNulo(QVariant(vazioNaoNulo), &ok) == 0 && ok);
        r.verificar(QStringLiteral("ligar(QVariant()) continua sendo NULL de propósito"), eNulo(QVariant(), &ok) == 1 && ok);
    }

    r.titulo(QStringLiteral("Repositórios (inserir, listar, filtrar, apagar)"));

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

    // --- Turmas e alunos ---
    Turma t;
    t.nome = QStringLiteral("8º A");
    t.disciplina = QStringLiteral("Matemática");
    t.anoLetivo = 2026;
    const int turmaId = turmas.inserir(t);
    r.verificar(QStringLiteral("TurmaRepository::inserir"), turmaId > 0, turmas.ultimoErro());
    r.verificar(QStringLiteral("TurmaRepository::listar"), turmas.listar(false).size() == 1, turmas.ultimoErro());

    Aluno a;
    a.turmaId = turmaId;
    a.nome = QStringLiteral("Ana Souza");
    a.matricula = QStringLiteral("2026001");
    const int alunoId = alunos.inserir(a);
    r.verificar(QStringLiteral("AlunoRepository::inserir"), alunoId > 0, alunos.ultimoErro());
    r.verificar(QStringLiteral("AlunoRepository::listarPorTurma (com filtro)"),
                alunos.listarPorTurma(turmaId, QStringLiteral("ana"), true).size() == 1, alunos.ultimoErro());
    Aluno repetido = a;  // mesma matrícula na mesma turma deve ser recusada
    r.verificar(QStringLiteral("matrícula repetida é recusada (índice único)"), alunos.inserir(repetido) == 0);

    // --- Avaliações e notas ---
    Avaliacao av;
    av.turmaId = turmaId;
    av.nome = QStringLiteral("Prova 1");
    av.peso = 2.0;
    av.data = QDate(2026, 10, 20);
    const int avId = avaliacoes.inserir(av);
    r.verificar(QStringLiteral("AvaliacaoRepository::inserir"), avId > 0, avaliacoes.ultimoErro());
    av.nome = QStringLiteral("Trabalho");
    av.data = QDate();
    const int avId2 = avaliacoes.inserir(av);
    r.verificar(QStringLiteral("AvaliacaoRepository::mover"), avaliacoes.mover(avId2, -1), avaliacoes.ultimoErro());
    r.verificar(QStringLiteral("AvaliacaoRepository::listarPorTurma (ordem após mover)"),
                avaliacoes.listarPorTurma(turmaId, 0).value(0).id == avId2);  // value(): seguro se a lista vier vazia

    r.verificar(QStringLiteral("NotaRepository::salvar"), notas.salvar(avId, alunoId, 8.5), notas.ultimoErro());
    r.verificar(QStringLiteral("NotaRepository::salvar (atualizar)"), notas.salvar(avId, alunoId, 9.0), notas.ultimoErro());
    r.verificar(QStringLiteral("NotaRepository::listarPorTurma"),
                notas.listarPorTurma(turmaId).value(NotaRepository::chave(avId, alunoId)) == 9.0);
    r.verificar(QStringLiteral("NotaRepository::contarAcima"), notas.contarAcima(avId, 8.0) == 1);

    // --- Horário e agenda ---
    Horario h;
    h.turmaId = turmaId;
    h.diaSemana = 1;
    h.inicio = QTime(7, 30);
    h.fim = QTime(8, 20);
    r.verificar(QStringLiteral("HorarioRepository::inserir"), horarios.inserir(h) > 0, horarios.ultimoErro());
    r.verificar(QStringLiteral("HorarioRepository::temConflito (sobrepõe)"), horarios.temConflito(1, QTime(8, 0), QTime(9, 0)));
    r.verificar(QStringLiteral("HorarioRepository::temConflito (encostado não conflita)"), !horarios.temConflito(1, QTime(8, 20), QTime(9, 10)));
    r.verificar(QStringLiteral("AgendaRepository::aulasDoDia (segunda, 05/10/2026)"),
                agenda.aulasDoDia(QDate(2026, 10, 5)).size() == 1, agenda.ultimoErro());
    r.verificar(QStringLiteral("AgendaRepository::avaliacoesDatadas"),
                agenda.avaliacoesDatadas(QDate(2026, 10, 1), QDate(2026, 10, 31)).size() == 1, agenda.ultimoErro());

    // --- Tarefas e calendário ---
    Tarefa tf;
    tf.titulo = QStringLiteral("Corrigir provas");
    tf.dataEntrega = QDate(2026, 10, 10);
    const int tarefaId = tarefas.inserir(tf);
    r.verificar(QStringLiteral("TarefaRepository::inserir (sem turma = NULL)"), tarefaId > 0, tarefas.ultimoErro());
    r.verificar(QStringLiteral("TarefaRepository::listarPendentes"), tarefas.listarPendentes().size() == 1, tarefas.ultimoErro());
    r.verificar(QStringLiteral("TarefaRepository::listarComPrazo"),
                tarefas.listarComPrazo(QDate(2026, 10, 1), QDate(2026, 10, 31)).size() == 1, tarefas.ultimoErro());
    r.verificar(QStringLiteral("TarefaRepository::marcarConcluida"), tarefas.marcarConcluida(tarefaId, true) && tarefas.listarPendentes().isEmpty());

    Evento e;
    e.titulo = QStringLiteral("Prova de Matemática");
    e.tipo = QStringLiteral("prova");
    e.turmaId = turmaId;
    e.dataInicio = QDate(2026, 10, 15);
    r.verificar(QStringLiteral("EventoRepository::inserir"), eventos.inserir(e) > 0, eventos.ultimoErro());
    Evento feriado;
    feriado.titulo = QStringLiteral("Feriado");
    feriado.tipo = QStringLiteral("feriado");
    feriado.dataInicio = QDate(2026, 10, 12);
    feriado.dataFim = QDate(2026, 10, 13);
    eventos.inserir(feriado);
    r.verificar(QStringLiteral("EventoRepository::listarPeriodo"), eventos.listarPeriodo(QDate(2026, 10, 1), QDate(2026, 10, 31)).size() == 2, eventos.ultimoErro());
    r.verificar(QStringLiteral("EventoRepository::motivoDeDiaSemAula (evento de vários dias)"), eventos.motivoDeDiaSemAula(QDate(2026, 10, 13)) == QLatin1String("Feriado"));
    r.verificar(QStringLiteral("AgendaRepository::provasProximas (evento + avaliação)"),
                agenda.provasProximas(QDate(2026, 10, 1), 30).size() == 2, agenda.ultimoErro());

    // --- Aulas, anexos, anotações ---
    Aula au;
    au.turmaId = turmaId;
    au.data = QDate(2026, 10, 5);
    au.tema = QStringLiteral("Frações");
    const int aulaId = aulas.inserir(au);
    r.verificar(QStringLiteral("AulaRepository::inserir/listar"), aulaId > 0 && aulas.listar(turmaId).size() == 1, aulas.ultimoErro());
    r.verificar(QStringLiteral("AgendaRepository::aulasDoDia traz o tema do plano"),
                agenda.aulasDoDia(QDate(2026, 10, 5)).value(0).tema == QLatin1String("Frações"));

    Anexo ax;
    ax.turmaId = turmaId;
    ax.aulaId = aulaId;
    ax.nome = QStringLiteral("slides.pptx");
    ax.caminho = QStringLiteral("C:/slides.pptx");
    ax.tipo = QStringLiteral("pptx");
    r.verificar(QStringLiteral("AnexoRepository::inserir"), anexos.inserir(ax) > 0, anexos.ultimoErro());
    r.verificar(QStringLiteral("AnexoRepository::listarPorAula / listarPorTurma"),
                anexos.listarPorAula(aulaId).size() == 1 && anexos.listarPorTurma(turmaId).size() == 1, anexos.ultimoErro());

    Anotacao an;
    an.titulo = QStringLiteral("Reunião de pais");
    an.conteudoHtml = QStringLiteral("<p>Falar sobre <b>recuperação</b></p>");
    an.conteudoTexto = QStringLiteral("Falar sobre recuperação");
    an.turmaId = turmaId;
    an.alunoId = alunoId;
    an.tags = {QStringLiteral("Prova"), QStringLiteral("prova"), QStringLiteral("reunião")};
    const int anId = anotacoes.inserir(an);
    r.verificar(QStringLiteral("AnotacaoRepository::inserir (com tags)"), anId > 0, anotacoes.ultimoErro());
    const auto lida = anotacoes.buscar(anId);
    r.verificar(QStringLiteral("AnotacaoRepository::buscar (tags sem repetição, ignorando maiúsculas)"), lida && lida->tags.size() == 2);
    AnotacaoRepository::Filtro f;
    f.tag = QStringLiteral("reunião");
    r.verificar(QStringLiteral("AnotacaoRepository::listar (filtro por tag)"), anotacoes.listar(f).size() == 1, anotacoes.ultimoErro());
    f = AnotacaoRepository::Filtro();
    f.texto = QStringLiteral("recuper");
    r.verificar(QStringLiteral("AnotacaoRepository::listar (busca no texto)"), anotacoes.listar(f).size() == 1, anotacoes.ultimoErro());
    r.verificar(QStringLiteral("AnotacaoRepository::listarTags"), anotacoes.listarTags().size() == 2);
    if (lida) {
        Anotacao editada = *lida;
        editada.tags = {QStringLiteral("reunião")};
        r.verificar(QStringLiteral("AnotacaoRepository::atualizar (remove tag órfã)"),
                    anotacoes.atualizar(editada) && anotacoes.listarTags().size() == 1, anotacoes.ultimoErro());
    }

    // --- Frequência ---
    const QDate dia(2026, 10, 5);
    r.verificar(QStringLiteral("FrequenciaRepository::salvar (presente)"), frequencia.salvar(alunoId, dia, QLatin1Char('P')), frequencia.ultimoErro());
    r.verificar(QStringLiteral("FrequenciaRepository::salvar (atualiza para falta)"), frequencia.salvar(alunoId, dia, QLatin1Char('F'), QStringLiteral("doente")), frequencia.ultimoErro());
    frequencia.salvar(alunoId, dia.addDays(1), QLatin1Char('P'));
    r.verificar(QStringLiteral("FrequenciaRepository::resumoPorAluno"),
                frequencia.resumoPorAluno(turmaId).value(alunoId).faltas == 1 && frequencia.resumoPorAluno(turmaId).value(alunoId).presencas == 1,
                frequencia.ultimoErro());
    r.verificar(QStringLiteral("FrequenciaRepository::doDia / doMes"),
                frequencia.doDia(turmaId, dia).size() == 1 && frequencia.doMes(turmaId, 2026, 10).size() == 2, frequencia.ultimoErro());
    r.verificar(QStringLiteral("FrequenciaRepository::marcarRestantes"), frequencia.marcarRestantes(turmaId, dia, QLatin1Char('P')), frequencia.ultimoErro());

    // --- Busca global, desempenho, relatórios ---
    r.verificar(QStringLiteral("BuscaRepository::carregarIndice"), busca.carregarIndice().size() >= 8, busca.ultimoErro());

    DesempenhoService servico(repos);
    const auto boletim = servico.boletim(turmaId, 0);
    r.verificar(QStringLiteral("DesempenhoService::boletim"), boletim && boletim->linhas.size() == 1 && boletim->linhas.first().media.has_value());
    const auto ficha = servico.ficha(alunoId);
    r.verificar(QStringLiteral("DesempenhoService::ficha"), ficha.has_value());
    if (boletim && ficha) {
        r.verificar(QStringLiteral("RelatorioPdf::html* (boletim, frequência e ficha)"),
                    RelatorioPdf::htmlBoletim(*boletim, 6.0).contains(QStringLiteral("Ana Souza")) &&
                        RelatorioPdf::htmlFrequencia(*boletim).contains(QStringLiteral("Ana Souza")) &&
                        RelatorioPdf::htmlFicha(*ficha, 6.0).contains(QStringLiteral("Ana Souza")));
    }

    // --- Backup ---
    const QString copia = pasta + QStringLiteral("/copia.db");
    QString erro;
    r.verificar(QStringLiteral("BackupService::criarBackup (VACUUM INTO)"), BackupService::criarBackup(copia, &erro), erro);
    r.verificar(QStringLiteral("BackupService::validarArquivo"), BackupService::validarArquivo(copia, &erro), erro);
    r.verificar(QStringLiteral("BackupService::validarArquivo recusa arquivo que não é banco"),
                !BackupService::validarArquivo(pasta + QStringLiteral("/nao-existe.db"), &erro));

    // --- Exclusão em cascata (depende de PRAGMA foreign_keys = ON) ---
    r.verificar(QStringLiteral("TurmaRepository::remover"), turmas.remover(turmaId), turmas.ultimoErro());
    r.verificar(QStringLiteral("exclusão em cascata: alunos, avaliações, notas, aulas e anexos somem com a turma"),
                alunos.listarPorTurma(turmaId, QString(), true).isEmpty() && avaliacoes.listarPorTurma(turmaId, 0).isEmpty() &&
                    notas.listarPorTurma(turmaId).isEmpty() && aulas.listar(turmaId).isEmpty() && anexos.listarPorTurma(turmaId).isEmpty());
}

}  // namespace

int executar(const QString &arquivoSaida)
{
    Relatorio r(arquivoSaida);
    r.linha(QStringLiteral("Autoteste do Professor Organizado"));
    r.linha(QStringLiteral("Versão: %1").arg(identificacaoDoBuild()));
    r.linha(QStringLiteral("Qt: %1 (compilado com %2)").arg(QString::fromLatin1(qVersion()), QStringLiteral(QT_VERSION_STR)));

    QTemporaryDir pasta;
    if (!pasta.isValid()) {
        r.verificar(QStringLiteral("criar pasta temporária"), false);
    } else {
        // 1) O caminho real do programa: DatabaseManager::abrir (PRAGMAs + migrações).
        r.titulo(QStringLiteral("Abertura do banco (DatabaseManager::abrir)"));
        r.info(QStringLiteral("pasta temporária: %1").arg(pasta.path()));
        DatabaseManager banco;
        const bool abriu = banco.abrir(pasta.path() + QStringLiteral("/professor.db"));
        r.verificar(QStringLiteral("DatabaseManager::abrir"), abriu, banco.ultimoErro());

        if (abriu) {
            QSqlDatabase db = QSqlDatabase::database();
            r.verificar(QStringLiteral("versão do esquema = %1").arg(Migrations::todas().last().versao),
                        Migrations::versaoAtual(db) == Migrations::todas().last().versao);
            QSqlQuery q(db);
            if (q.exec(QStringLiteral("SELECT sqlite_version()")) && q.next())
                r.info(QStringLiteral("SQLite %1").arg(q.value(0).toString()));
            q.finish();
            if (q.exec(QStringLiteral("PRAGMA journal_mode")) && q.next())
                r.info(QStringLiteral("journal_mode = %1").arg(q.value(0).toString()));
            q.finish();
            testeDeFumaca(r, pasta.path());
        }

        // 2) As variantes só interessam como diagnóstico; rodam sempre, mas são
        //    obrigatórias apenas quando a abertura principal falhou.
        rodarVariantes(r, pasta.path());

        // Fecha a conexão principal antes de a pasta temporária ser apagada
        // (no Windows, arquivo aberto não pode ser removido).
        if (abriu)
            QSqlDatabase::database().close();
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    }

    r.linha(QString());
    r.linha(r.falhas == 0 ? QStringLiteral("RESULTADO: tudo certo.")
                          : QStringLiteral("RESULTADO: %1 verificação(ões) falharam.").arg(r.falhas));
    r.arquivo.close();
    return r.falhas == 0 ? 0 : 1;
}

}  // namespace AutoTeste
