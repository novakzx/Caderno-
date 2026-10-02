#include "services/AutoTeste.h"

#include "core/BuildInfo.h"
#include "core/AnexoUtil.h"
#include "core/Tokens.h"
#include "database/AgendaRepository.h"
#include "database/AlunoRepository.h"
#include "database/AnexoRepository.h"
#include "database/AnotacaoRepository.h"
#include "database/AulaRepository.h"
#include "database/AvaliacaoRepository.h"
#include "database/BuscaRepository.h"
#include "database/ContaRepository.h"
#include "database/DatabaseManager.h"
#include "database/EventoRepository.h"
#include "database/FrequenciaRepository.h"
#include "database/HorarioRepository.h"
#include "database/Migrations.h"
#include "database/NotaRepository.h"
#include "database/OcorrenciaRepository.h"
#include "database/Repositorios.h"
#include "database/SqlUtil.h"
#include "database/TarefaRepository.h"
#include "database/TurmaRepository.h"
#include "services/BackupService.h"
#include "services/ContaService.h"
#include "services/AtualizacaoService.h"
#include "services/CalendarioExport.h"
#include "services/DesempenhoService.h"
#include "services/IaPrompts.h"
#include "services/IaService.h"
#include "services/SegredoService.h"
#include "services/ImportadorAlunos.h"
#include "services/LembreteService.h"
#include "services/XlsxService.h"
#include "services/RelatorioPdf.h"

#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QUuid>

#include <algorithm>

#include <xlsxcell.h>
#include <xlsxdocument.h>
#include <xlsxformat.h>

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
    OcorrenciaRepository ocorrencias;
    Repositorios repos{turmas, alunos, avaliacoes, notas, horarios, tarefas, agenda,
                       aulas, anexos, anotacoes, frequencia, eventos, busca, ocorrencias};

    // --- Turmas e alunos ---
    Turma t;
    t.nome = QStringLiteral("8º A");
    t.disciplina = QStringLiteral("Matemática");
    t.anoLetivo = 2026;
    const int turmaId = turmas.inserir(t);
    r.verificar(QStringLiteral("TurmaRepository::inserir"), turmaId > 0, turmas.ultimoErro());
    r.verificar(QStringLiteral("TurmaRepository::listar"), turmas.listar(false).size() == 1, turmas.ultimoErro());
    r.verificar(QStringLiteral("cor padrão da turma nova = turma-6 (token do design)"),
                turmas.listar(false).value(0).cor == QStringLiteral("turma-6"), turmas.listar(false).value(0).cor);

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
                agenda.aulasDoDia(QDate(2026, 10, 5)).value(0).tema == QStringLiteral("Frações"));  // QStringLiteral (UTF-8), não QLatin1String

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

    // --- Lembretes (segunda, 05/10/2026: há aula às 07:30) ---
    {
        LembreteService lembretes(agenda, tarefas);
        LembreteUtil::Config cfg;  // padrão: 10 min antes, prazos ligados
        const QDate segunda(2026, 10, 5);
        r.verificar(QStringLiteral("LembreteService: 07:25 avisa da aula das 07:30 (faltam 5 min)"),
                    [&] {
                        const auto l = lembretes.pendentes(QDateTime(segunda, QTime(7, 25)), cfg);
                        return l.size() == 1 && l.first().titulo == QStringLiteral("Aula em 5 minutos") &&
                               l.first().chave.startsWith(QStringLiteral("aula:")) && l.first().texto.contains(QStringLiteral("8º A"));
                    }());
        r.verificar(QStringLiteral("LembreteService: 07:10 (cedo) e 07:30 (já começou) não avisam"),
                    lembretes.pendentes(QDateTime(segunda, QTime(7, 10)), cfg).isEmpty() &&
                        lembretes.pendentes(QDateTime(segunda, QTime(7, 30)), cfg).isEmpty());
        LembreteUtil::Config desligado = cfg;
        desligado.ativos = false;
        LembreteUtil::Config semAulas = cfg;
        semAulas.antecedenciaAula = 0;
        r.verificar(QStringLiteral("LembreteService: desligado ou sem aulas não avisa"),
                    lembretes.pendentes(QDateTime(segunda, QTime(7, 25)), desligado).isEmpty() &&
                        lembretes.pendentes(QDateTime(segunda, QTime(7, 25)), semAulas).isEmpty());

        // Tarefa e prova para amanhã (06/10): só avisam a partir das 8h.
        Tarefa amanha;
        amanha.titulo = QStringLiteral("Entregar notas");
        amanha.dataEntrega = QDate(2026, 10, 6);
        const int amanhaId = tarefas.inserir(amanha);
        Evento provaAmanha;
        provaAmanha.titulo = QStringLiteral("Prova de Ciências");
        provaAmanha.tipo = QStringLiteral("prova");
        provaAmanha.dataInicio = QDate(2026, 10, 6);
        const int provaAmanhaId = eventos.inserir(provaAmanha);
        r.verificar(QStringLiteral("LembreteService: tarefa e prova de amanhã não avisam de madrugada (07:25)"),
                    lembretes.pendentes(QDateTime(segunda, QTime(7, 25)), cfg).size() == 1);  // só a aula
        const auto as9 = lembretes.pendentes(QDateTime(segunda, QTime(9, 0)), cfg);
        r.verificar(QStringLiteral("LembreteService: às 9h avisa da tarefa e da prova de amanhã (e não da tarefa já concluída)"),
                    as9.size() == 2 && as9.first().chave != as9.last().chave, QString::number(as9.size()));
        LembreteUtil::Config semPrazos = cfg;
        semPrazos.prazos = false;
        r.verificar(QStringLiteral("LembreteService: com prazos desligados não avisa de tarefa nem prova"),
                    lembretes.pendentes(QDateTime(segunda, QTime(9, 0)), semPrazos).isEmpty());
        r.verificar(QStringLiteral("LembreteService: tarefa atrasada não avisa (dia 07/10)"),
                    lembretes.pendentes(QDateTime(QDate(2026, 10, 7), QTime(9, 0)), cfg).isEmpty());
        // A chave muda de um dia para o outro (o aviso de "hoje" é novo, não repete o de "amanhã").
        const auto noDia = lembretes.pendentes(QDateTime(QDate(2026, 10, 6), QTime(9, 0)), cfg);
        r.verificar(QStringLiteral("LembreteService: no próprio dia avisa de novo, com outra chave"),
                    noDia.size() == 2 && !as9.isEmpty() && noDia.first().chave != as9.first().chave && noDia.first().titulo.contains(QStringLiteral("hoje")));
        tarefas.remover(amanhaId);
        eventos.remover(provaAmanhaId);
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

    // --- Ocorrências por aluno ---
    Ocorrencia oc;
    oc.alunoId = alunoId;
    oc.data = QDate::currentDate();
    oc.tipo = QStringLiteral("conduta");
    oc.texto = QStringLiteral("Conversou durante a prova <b>x</b>");  // HTML digitado pelo usuário: nunca pode virar HTML
    const int ocId = ocorrencias.inserir(oc);
    r.verificar(QStringLiteral("OcorrenciaRepository::inserir"), ocId > 0, ocorrencias.ultimoErro());
    Ocorrencia oc2 = oc;
    oc2.data = QDate::currentDate().addDays(-3);
    oc2.tipo = QStringLiteral("tipo-que-nao-existe");
    oc2.texto = QStringLiteral("Anotação antiga");
    const int oc2Id = ocorrencias.inserir(oc2);
    r.verificar(QStringLiteral("OcorrenciaRepository::inserir (tipo desconhecido vira \"outro\")"),
                oc2Id > 0 && ocorrencias.buscar(oc2Id) && ocorrencias.buscar(oc2Id)->tipo == QStringLiteral("outro"), ocorrencias.ultimoErro());
    const QList<Ocorrencia> daAluna = ocorrencias.listarPorAluno(alunoId);
    r.verificar(QStringLiteral("OcorrenciaRepository::listarPorAluno (mais recentes primeiro)"),
                daAluna.size() == 2 && daAluna.first().id == ocId && daAluna.first().texto == oc.texto, ocorrencias.ultimoErro());
    Ocorrencia editada = daAluna.first();
    editada.tipo = QStringLiteral("elogio");
    r.verificar(QStringLiteral("OcorrenciaRepository::atualizar"),
                ocorrencias.atualizar(editada) && ocorrencias.buscar(ocId)->tipo == QStringLiteral("elogio"), ocorrencias.ultimoErro());
    r.verificar(QStringLiteral("contarNegativasPorAluno: elogio e \"outro\" não contam"),
                ocorrencias.contarNegativasPorAluno(turmaId, QDate::currentDate().addDays(-30)).value(alunoId, 0) == 0);
    editada.tipo = QStringLiteral("dificuldade");
    ocorrencias.atualizar(editada);
    r.verificar(QStringLiteral("contarNegativasPorAluno: conta só as negativas dentro do prazo"),
                ocorrencias.contarNegativasPorAluno(turmaId, QDate::currentDate().addDays(-30)).value(alunoId, 0) == 1 &&
                    ocorrencias.contarNegativasPorAluno(turmaId, QDate::currentDate().addDays(1)).isEmpty());
    r.verificar(QStringLiteral("OcorrenciaRepository::contarPorAluno"), ocorrencias.contarPorAluno(turmaId).value(alunoId, 0) == 2);
    r.verificar(QStringLiteral("OcorrenciaRepository::remover"),
                ocorrencias.remover(oc2Id) && !ocorrencias.buscar(oc2Id) && ocorrencias.listarPorAluno(alunoId).size() == 1, ocorrencias.ultimoErro());

    // --- Busca global, desempenho, relatórios ---
    r.verificar(QStringLiteral("BuscaRepository::carregarIndice"), busca.carregarIndice().size() >= 8, busca.ultimoErro());

    DesempenhoService servico(repos);
    const auto boletim = servico.boletim(turmaId, 0);
    r.verificar(QStringLiteral("DesempenhoService::boletim"), boletim && boletim->linhas.size() == 1 && boletim->linhas.first().media.has_value());
    const auto ficha = servico.ficha(alunoId);
    r.verificar(QStringLiteral("DesempenhoService::ficha"), ficha.has_value());
    r.verificar(QStringLiteral("DesempenhoService::ficha traz as ocorrências registradas"), ficha && ficha->historico.size() == 1);

    // Alunos em atenção: a aluna tem 1 falta em 2 chamadas (50% < 75%) e 1 ocorrência negativa recente.
    const QList<AlunoEmAtencao> emAtencao = servico.alunosEmAtencao(6.0, QDate::currentDate());
    r.verificar(QStringLiteral("DesempenhoService::alunosEmAtencao: frequência de 50% entra como urgente"),
                emAtencao.size() == 1 && emAtencao.first().alunoId == alunoId &&
                    emAtencao.first().nivel == AtencaoUtil::Nivel::Critico &&
                    std::find(emAtencao.first().motivos.begin(), emAtencao.first().motivos.end(), AtencaoUtil::Motivo::FrequenciaAbaixo) !=
                        emAtencao.first().motivos.end());
    r.verificar(QStringLiteral("alunosEmAtencao: ocorrências recentes contam (1 só ainda não alerta)"),
                emAtencao.size() == 1 && emAtencao.first().ocorrenciasNegativas == 1 &&
                    std::find(emAtencao.first().motivos.begin(), emAtencao.first().motivos.end(), AtencaoUtil::Motivo::Ocorrencias) ==
                        emAtencao.first().motivos.end());
    if (boletim && ficha) {
        r.verificar(QStringLiteral("RelatorioPdf::html* (boletim, frequência e ficha)"),
                    RelatorioPdf::htmlBoletim(*boletim, 6.0).contains(QStringLiteral("Ana Souza")) &&
                        RelatorioPdf::htmlFrequencia(*boletim).contains(QStringLiteral("Ana Souza")) &&
                        RelatorioPdf::htmlFicha(*ficha, 6.0).contains(QStringLiteral("Ana Souza")));
        const QString htmlDaFicha = RelatorioPdf::htmlFicha(*ficha, 6.0);
        r.verificar(QStringLiteral("ficha em PDF lista as ocorrências, com o texto do usuário escapado (sem HTML ativo)"),
                    htmlDaFicha.contains(QStringLiteral("Ocorrências registradas")) &&
                        htmlDaFicha.contains(QStringLiteral("Conversou durante a prova &lt;b&gt;x&lt;/b&gt;")) &&
                        !htmlDaFicha.contains(QStringLiteral("<b>x</b>")));
    }

    // --- Backup ---
    const QString copia = pasta + QStringLiteral("/copia.db");
    QString erro;
    r.verificar(QStringLiteral("BackupService::criarBackup (VACUUM INTO)"), BackupService::criarBackup(copia, &erro), erro);
    r.verificar(QStringLiteral("BackupService::validarArquivo"), BackupService::validarArquivo(copia, &erro), erro);
    r.verificar(QStringLiteral("BackupService::validarArquivo recusa arquivo que não é banco"),
                !BackupService::validarArquivo(pasta + QStringLiteral("/nao-existe.db"), &erro));

    // --- Exportação do calendário (.ics) ---
    {
        int totalIcs = 0;
        const QDateTime geradoEm(QDate(2026, 10, 2), QTime(12, 0), Qt::UTC);
        Evento malicioso;  // título com quebra de linha tentando criar um evento falso no arquivo
        malicioso.titulo = QStringLiteral("Reunião\r\nBEGIN:VEVENT\r\nSUMMARY:falso");
        malicioso.tipo = QStringLiteral("reuniao");
        malicioso.dataInicio = QDate(2026, 10, 20);
        const int maliciosoId = eventos.inserir(malicioso);
        const QByteArray ics = CalendarioExport::gerarIcs(eventos, tarefas, agenda, QDate(2026, 10, 1), QDate(2026, 10, 31), geradoEm, &totalIcs);
        const QString texto = QString::fromUtf8(ics);
        r.verificar(QStringLiteral("CalendarioExport: eventos, feriado de vários dias e avaliação entram; tarefa concluída não"),
                    totalIcs == 4 && texto.startsWith(QStringLiteral("BEGIN:VCALENDAR\r\n")) && texto.contains(QStringLiteral("SUMMARY:Prova: Prova de Matem")) &&
                        texto.contains(QStringLiteral("DTEND;VALUE=DATE:20261014\r\n")) && texto.contains(QStringLiteral("DTSTAMP:20261002T120000Z")) &&
                        !texto.contains(QStringLiteral("Corrigir provas")),
                    QString::number(totalIcs));
        r.verificar(QStringLiteral("CalendarioExport: quebra de linha no título NÃO cria evento falso (4 eventos, nenhum a mais)"),
                    texto.count(QStringLiteral("\r\nBEGIN:VEVENT\r\n")) == 4 && !texto.contains(QStringLiteral("\r\nSUMMARY:falso")));
        if (maliciosoId > 0)
            eventos.remover(maliciosoId);
    }

    // --- Importação de alunos (CSV / lista colada) ---
    {
        ImportadorAlunos importador(alunos);
        using S = LinhaImportacaoAluno::Situacao;
        QString erroLista;

        // Arquivo-modelo: Ana (já está na turma, mesma matrícula) e Bruno (novo).
        const auto modelo = ImportadorAlunos::tabelaDeTexto(ImportadorAlunos::modeloCsv(), &erroLista);
        const PlanoImportacaoAlunos planoModelo = modelo ? importador.planejar(turmaId, *modelo) : PlanoImportacaoAlunos();
        r.verificar(QStringLiteral("ImportadorAlunos: o arquivo-modelo é lido (BOM, ';', acentos) e Ana já existe"),
                    modelo && planoModelo.erro.isEmpty() && planoModelo.linhas.size() == 2 &&
                        planoModelo.linhas.at(0).situacao == S::JaExiste && planoModelo.linhas.at(1).situacao == S::Nova &&
                        planoModelo.linhas.at(1).aluno.dataNascimento == QDate(2011, 11, 2),
                    erroLista + planoModelo.erro);

        const QByteArray csv = QStringLiteral(
                                   "Nome;Matrícula;E-mail;Nascimento\r\n"
                                   "Ana Souza;2026001;;\r\n"
                                   "Bruno Lima;2026002;bruno@escola.com;15/03/2012\r\n"
                                   "\"Souza, Carla\";2026003;carla-sem-arroba;31/02/2012\r\n"
                                   "Bruno Lima;2026002;;\r\n"
                                   ";2026009;;\r\n"
                                   "João da Silva;;;\r\n"
                                   "joao da silva;;;\r\n"
                                   "\"=HYPERLINK(\"\"http://x\"\")\";2026010;;\r\n")
                                   .toUtf8();
        const auto tabela = ImportadorAlunos::tabelaDeTexto(csv, &erroLista);
        const PlanoImportacaoAlunos plano = tabela ? importador.planejar(turmaId, *tabela) : PlanoImportacaoAlunos();
        r.verificar(QStringLiteral("ImportadorAlunos::planejar: 4 novos, 1 já existe, 2 repetidos, 1 inválido"),
                    plano.erro.isEmpty() && plano.novos() == 4 && plano.total(S::JaExiste) == 1 &&
                        plano.total(S::RepetidaNoArquivo) == 2 && plano.total(S::Invalida) == 1,
                    erroLista + plano.erro);
        r.verificar(QStringLiteral("ImportadorAlunos: e-mail e data inválidos viram aviso (a linha continua)"),
                    plano.linhas.size() == 8 && plano.linhas.at(2).situacao == S::Nova && plano.linhas.at(2).aluno.nome == QStringLiteral("Souza, Carla") &&
                        plano.linhas.at(2).aluno.email.isEmpty() && !plano.linhas.at(2).aluno.dataNascimento.isValid() &&
                        plano.linhas.at(2).detalhe.contains(QStringLiteral("e-mail")) && plano.linhas.at(2).detalhe.contains(QStringLiteral("data")));

        int criados = 0;
        const bool gravou = importador.executar(turmaId, plano, &criados, &erroLista);
        r.verificar(QStringLiteral("ImportadorAlunos::executar grava só os novos (transação única)"),
                    gravou && criados == 4 && alunos.listarPorTurma(turmaId, QString(), true).size() == 5, erroLista);
        QStringList nomes;
        for (const Aluno &a : alunos.listarPorTurma(turmaId, QString(), true))
            nomes << a.nome;
        r.verificar(QStringLiteral("ImportadorAlunos: texto que parece fórmula entra como texto puro"),
                    nomes.contains(QStringLiteral("=HYPERLINK(\"http://x\")")) && nomes.contains(QStringLiteral("João da Silva")));
        r.verificar(QStringLiteral("ImportadorAlunos: importar de novo a mesma lista não duplica ninguém"),
                    importador.planejar(turmaId, *tabela).novos() == 0);

        // Lista de uma coluna, sem título; Windows-1252 ("João" em Latin-1); sem cabeçalho de verdade = erro claro.
        const QByteArray latin1 = QByteArray("Maria Alves\nJo") + QByteArray::fromHex("E3") + QByteArray("o Perez\n");  // 0xE3 = "ã" em Windows-1252
        const auto soNomes = ImportadorAlunos::tabelaDeTexto(latin1, &erroLista);
        const PlanoImportacaoAlunos planoNomes = soNomes ? importador.planejar(turmaId, *soNomes) : PlanoImportacaoAlunos();
        r.verificar(QStringLiteral("ImportadorAlunos: lista só de nomes (sem título) e arquivo em Windows-1252"),
                    planoNomes.erro.isEmpty() && !planoNomes.temCabecalho && planoNomes.novos() == 2 &&
                        planoNomes.linhas.at(1).aluno.nome == QStringLiteral("João Perez"),
                    erroLista + planoNomes.erro);
        const auto semTitulo = ImportadorAlunos::tabelaDeTexto(QByteArray("Ana;1\nBia;2\n"), &erroLista);
        r.verificar(QStringLiteral("ImportadorAlunos: várias colunas sem títulos reconhecíveis = erro explicado"),
                    semTitulo && !importador.planejar(turmaId, *semTitulo).erro.isEmpty());
        const auto semNome = ImportadorAlunos::tabelaDeTexto(QByteArray("Matrícula;E-mail\n1;a@b.com\n"), &erroLista);
        r.verificar(QStringLiteral("ImportadorAlunos: títulos sem a coluna Nome = erro explicado"),
                    semNome && importador.planejar(turmaId, *semNome).erro.contains(QStringLiteral("coluna de nomes")));
        r.verificar(QStringLiteral("ImportadorAlunos: texto vazio e arquivo enorme são recusados"),
                    !ImportadorAlunos::tabelaDeTexto(QByteArray("  \n \n"), &erroLista) &&
                        !ImportadorAlunos::tabelaDeTexto(QByteArray(ImportadorAlunos::kTamanhoMaximoDoTexto + 1, 'a'), &erroLista));
        r.verificar(QStringLiteral("ImportadorAlunos: arquivo .xls antigo é recusado com orientação"),
                    !ImportadorAlunos::tabelaDeArquivo(QStringLiteral("lista.xls"), &erroLista) && erroLista.contains(QStringLiteral(".xlsx")));

        // Excel (.xlsx): matrícula numérica vira texto sem ".0" e a célula de data vira uma data de verdade.
        const QString caminhoXlsx = pasta + QStringLiteral("/lista-alunos.xlsx");
        {
            QXlsx::Document doc;
            doc.write(1, 1, QStringLiteral("Aluno"));
            doc.write(1, 2, QStringLiteral("RA"));
            doc.write(1, 3, QStringLiteral("Data de nascimento"));
            doc.write(2, 1, QStringLiteral("Davi Costa"));
            doc.write(2, 2, 2026777);
            QXlsx::Format formatoData;
            formatoData.setNumberFormat(QStringLiteral("dd/mm/yyyy"));
            doc.write(2, 3, QDate(2012, 5, 20), formatoData);
            r.verificar(QStringLiteral("preparar a lista em .xlsx"), doc.saveAs(caminhoXlsx));
        }
        const auto tabelaXlsx = ImportadorAlunos::tabelaDeArquivo(caminhoXlsx, &erroLista);
        const PlanoImportacaoAlunos planoXlsx = tabelaXlsx ? importador.planejar(turmaId, *tabelaXlsx) : PlanoImportacaoAlunos();
        r.verificar(QStringLiteral("ImportadorAlunos: lista em .xlsx (títulos \"Aluno\", \"RA\", \"Data de nascimento\")"),
                    planoXlsx.erro.isEmpty() && planoXlsx.novos() == 1 && planoXlsx.linhas.first().aluno.nome == QStringLiteral("Davi Costa") &&
                        planoXlsx.linhas.first().aluno.matricula == QStringLiteral("2026777") &&
                        planoXlsx.linhas.first().aluno.dataNascimento == QDate(2012, 5, 20),
                    erroLista + planoXlsx.erro);
    }

    // --- Exclusão em cascata (depende de PRAGMA foreign_keys = ON) ---
    r.verificar(QStringLiteral("TurmaRepository::remover"), turmas.remover(turmaId), turmas.ultimoErro());
    r.verificar(QStringLiteral("exclusão em cascata: alunos, avaliações, notas, aulas e anexos somem com a turma"),
                alunos.listarPorTurma(turmaId, QString(), true).isEmpty() && avaliacoes.listarPorTurma(turmaId, 0).isEmpty() &&
                    notas.listarPorTurma(turmaId).isEmpty() && aulas.listar(turmaId).isEmpty() && anexos.listarPorTurma(turmaId).isEmpty() &&
                    ocorrencias.listarPorAluno(alunoId).isEmpty());
}

// ---------------------------------------------------------------------------
// Migração 5: turmas com o azul antigo (#4C8BF5) passam a "turma-6" (padrão do design);
// cores escolhidas à mão continuam como estão.
// ---------------------------------------------------------------------------
void testeMigracaoDeCores(Relatorio &r, const QString &pasta)
{
    r.titulo(QStringLiteral("Migração 5 (cor da turma como token do design)"));
    const QString conexao = QStringLiteral("autoteste_migracao5");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conexao);
        db.setDatabaseName(pasta + QStringLiteral("/migracao5.db"));
        QString erro;
        bool ok = db.open() && Migrations::aplicar(db, &erro);
        r.verificar(QStringLiteral("banco novo migra até a versão final"), ok, erro);

        if (ok) {
            // Simula um banco da versão 4, com turmas do padrão antigo e com cor própria.
            ok = Migrations::executarComando(db, QStringLiteral("INSERT INTO turmas (nome, ano_letivo, cor) VALUES ('Padrao antigo', 2026, '#4c8bf5')"), &erro) &&
                 Migrations::executarComando(db, QStringLiteral("INSERT INTO turmas (nome, ano_letivo, cor) VALUES ('Cor propria', 2026, '#112233')"), &erro) &&
                 // Desfaz o que migrações posteriores à 4 criaram (a 6 criou "ocorrencias"), para simular um banco antigo de verdade.
                 Migrations::executarComando(db, QStringLiteral("DROP TABLE ocorrencias"), &erro) &&
                 Migrations::executarComando(db, QStringLiteral("PRAGMA user_version = 4"), &erro);
            r.verificar(QStringLiteral("preparar banco na versão 4"), ok, erro);
        }
        if (ok) {
            ok = Migrations::aplicar(db, &erro);
            r.verificar(QStringLiteral("migração 5 aplicada sobre o banco antigo"), ok && Migrations::versaoAtual(db) == Migrations::todas().last().versao, erro);

            QSqlQuery q(db);
            QString padraoAntigo, propria;
            if (q.exec(QStringLiteral("SELECT nome, cor FROM turmas ORDER BY id"))) {
                while (q.next())
                    (q.value(0).toString().startsWith(QStringLiteral("Padrao")) ? padraoAntigo : propria) = q.value(1).toString();
            }
            q.finish();
            r.verificar(QStringLiteral("#4C8BF5 (padrão antigo) virou turma-6"), padraoAntigo == QStringLiteral("turma-6"), padraoAntigo);
            r.verificar(QStringLiteral("cor escolhida à mão não foi alterada"), propria == QStringLiteral("#112233"), propria);
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(conexao);
}

// ---------------------------------------------------------------------------
// Recursos do design embutidos no .exe: ícones SVG da interface e fontes Figtree.
// (As fontes são só conferidas como arquivo; carregá-las exige a interface gráfica.)
// ---------------------------------------------------------------------------
void testeRecursosDoDesign(Relatorio &r)
{
    r.titulo(QStringLiteral("Recursos do design (ícones SVG, fontes, tokens)"));

    // Todos os SVG embutidos devem ser válidos e usar currentColor (para o tema recolorir),
    // exceto o símbolo da marca, que tem as cores próprias do logo.
    int validos = 0;
    QStringList invalidos;
    const QStringList arquivos = QDir(QStringLiteral(":/icons")).entryList({QStringLiteral("*.svg")}, QDir::Files);
    for (const QString &nomeArquivo : arquivos) {
        QFile arquivo(QStringLiteral(":/icons/%1").arg(nomeArquivo));
        const bool existe = arquivo.open(QIODevice::ReadOnly);
        const QByteArray svg = existe ? arquivo.readAll() : QByteArray();
        QSvgRenderer renderizador(svg);
        const bool exigeCor = nomeArquivo != QLatin1String("caderno-mark.svg");
        if (existe && renderizador.isValid() && (!exigeCor || svg.contains("currentColor")))
            ++validos;
        else
            invalidos << nomeArquivo;
    }
    r.verificar(QStringLiteral("ícones SVG embutidos e válidos (%1 de %2)").arg(validos).arg(arquivos.size()),
                invalidos.isEmpty() && arquivos.size() >= 40, invalidos.join(QStringLiteral(", ")));
    // Ícones que o código pede pelo nome: nenhum pode faltar.
    static const char *usados[] = {"hoje", "turmas", "notas", "frequencia", "horario", "aulas", "anotacoes", "tarefas",
                                   "calendario", "relatorios", "busca", "backup", "lua", "sol", "check", "anexo",
                                   "marca-texto", "limpar", "aluno", "tarefa-ok", "feriado", "recesso", "pin", "mais",
                                   "subir", "baixar", "imagem", "usuario", "cadeado", "email", "olho", "olho-fechado",
                                   "sair", "chave", "janela-minimizar", "janela-maximizar", "janela-restaurar",
                                   "janela-fechar", "seta-baixo", "seta-cima", "seta-esquerda", "seta-direita", "marca",
                                   "caderno-mark", "sino", "assistente", "ajustes", "dado", "copiar"};
    QStringList faltando;
    for (const char *nome : usados)
        if (!QFile::exists(QStringLiteral(":/icons/%1.svg").arg(QLatin1String(nome))))
            faltando << QLatin1String(nome);
    r.verificar(QStringLiteral("todos os ícones usados pelo programa existem"), faltando.isEmpty(), faltando.join(QStringLiteral(", ")));

    for (const char *fonte : {"Figtree-Regular.ttf", "Figtree-SemiBold.ttf", "Figtree-Bold.ttf"}) {
        QFile arquivo(QStringLiteral(":/fonts/%1").arg(QLatin1String(fonte)));
        const bool ok = arquivo.open(QIODevice::ReadOnly) && arquivo.size() > 20000 && arquivo.read(4) == QByteArray("\x00\x01\x00\x00", 4);
        r.verificar(QStringLiteral("fonte embutida: %1").arg(QLatin1String(fonte)), ok);
    }

    // Tokens: nomes na ordem do enum, turma-6 = primary, cores no formato #rrggbb.
    bool formatoOk = true;
    for (std::size_t i = 0; i < Tokens::kTotal; ++i) {
        const auto id = static_cast<Tokens::Id>(i);
        for (const bool escuro : {false, true})
            formatoOk = formatoOk && QColor(QLatin1String(Tokens::hex(id, escuro))).isValid();
    }
    r.verificar(QStringLiteral("%1 tokens de cor válidos nos dois temas").arg(int(Tokens::kTotal)), formatoOk);
    r.verificar(QStringLiteral("cor padrão de turma nova (turma-6) é o primary"),
                QLatin1String(Tokens::hex(Tokens::Id::Turma6, false)) == QLatin1String(Tokens::hex(Tokens::Id::Primary, false)));
}

// ---------------------------------------------------------------------------
// Contas locais: cadastro, entrada, bloqueio por tentativas, recuperação de senha.
// O relógio é simulado para testar o bloqueio sem esperar.
// ---------------------------------------------------------------------------
void testeContas(Relatorio &r, const QString &pasta)
{
    r.titulo(QStringLiteral("Contas (login, senha, bloqueio, recuperação)"));

    // PBKDF2-HMAC-SHA512 contra vetores conhecidos (gerados com o hashlib do Python).
    const QByteArray senhaV("password"), salV("salt");
    r.verificar(QStringLiteral("PBKDF2-SHA512, 1 iteração (vetor conhecido)"),
                ContaService::pbkdf2(senhaV, salV, 1).toHex() ==
                    "867f70cf1ade02cff3752599a3a53dc4af34c7a669815ae5d513554e1c8cf252c02d470a285a0501bad999bfe943c08f050235d7d68b1da55e63f73b60a57fce");
    r.verificar(QStringLiteral("PBKDF2-SHA512, 2 iterações (vetor conhecido)"),
                ContaService::pbkdf2(senhaV, salV, 2).toHex() ==
                    "e1d9c16aa681708a45f5c7c4e215ceb66e011a2e9f0040713f18aefdb866d53cf76cab2868a39b9f7840edce4fef5a82be67335c77a6068e04112754f27ccf4e");
    r.verificar(QStringLiteral("PBKDF2-SHA512, 4096 iterações (vetor conhecido)"),
                ContaService::pbkdf2(senhaV, salV, 4096).toHex() ==
                    "d197b1b33db0143e018b12f3d1d1479e6cdebdcc97c5c0f87f6902e072f457b5143f30602641b3d55cd335988cb36b84376060ecd532e039b742a239434af2d5");

    qint64 agoraDeTeste = 1000000;
    ContaService::usarRelogioDeTeste([&agoraDeTeste] { return agoraDeTeste; });
    const QString pastaContas = pasta + QStringLiteral("/contas-teste");
    {
        ContaService contas(pastaContas);
        r.verificar(QStringLiteral("cadastro de contas abre"), contas.disponivel(), contas.erroDeAbertura());
        r.verificar(QStringLiteral("começa sem contas"), !contas.temContas());

        const QString senha = QStringLiteral("Giz2026!x");
        QElapsedTimer cronometro;
        cronometro.start();
        const ResultadoConta ana = contas.registrar(QStringLiteral("Ana Souza"), QStringLiteral("Ana@Escola.com"), senha, senha);
        r.info(QStringLiteral("criar conta (2 hashes PBKDF2 de 210 mil iterações): %1 ms").arg(cronometro.elapsed()));
        r.verificar(QStringLiteral("registrar: conta criada"), ana.ok, ana.erro);
        r.verificar(QStringLiteral("e-mail guardado em minúsculas; 1ª conta adota professor.db"),
                    ana.conta.email == QStringLiteral("ana@escola.com") && ana.conta.arquivoDados == QStringLiteral("professor.db"));
        r.verificar(QStringLiteral("código de recuperação no formato XXXX-XXXX-XXXX-XXXX-XXXX"),
                    QRegularExpression(QStringLiteral("^[A-Z2-9]{4}(-[A-Z2-9]{4}){4}$")).match(ana.codigoRecuperacao).hasMatch(),
                    ana.codigoRecuperacao);

        // Recusas de cadastro
        r.verificar(QStringLiteral("registrar recusa senha fraca"),
                    !contas.registrar(QStringLiteral("Bia Lima"), QStringLiteral("bia@x.com"), QStringLiteral("12345678"), QStringLiteral("12345678")).ok);
        r.verificar(QStringLiteral("registrar recusa confirmação diferente"),
                    !contas.registrar(QStringLiteral("Bia Lima"), QStringLiteral("bia@x.com"), senha, QStringLiteral("outra")).ok);
        r.verificar(QStringLiteral("registrar recusa e-mail inválido"),
                    !contas.registrar(QStringLiteral("Bia Lima"), QStringLiteral("bia-sem-arroba"), senha, senha).ok);
        r.verificar(QStringLiteral("registrar recusa e-mail repetido (ignora maiúsculas)"),
                    !contas.registrar(QStringLiteral("Outra Ana"), QStringLiteral("ANA@ESCOLA.COM"), senha, senha).ok);

        const ResultadoConta bia = contas.registrar(QStringLiteral("Bia Lima"), QStringLiteral("bia@escola.com"), QStringLiteral("Lousa#2027"), QStringLiteral("Lousa#2027"));
        r.verificar(QStringLiteral("2ª conta tem pasta de dados própria"),
                    bia.ok && bia.conta.arquivoDados == QStringLiteral("contas/%1/professor.db").arg(bia.conta.id), bia.erro + bia.conta.arquivoDados);

        // Entrar
        r.verificar(QStringLiteral("entrar com a senha certa (e-mail em maiúsculas)"), contas.entrar(QStringLiteral("ANA@escola.com"), senha).ok);
        const ResultadoConta errada = contas.entrar(QStringLiteral("ana@escola.com"), QStringLiteral("senha-errada1"));
        const ResultadoConta inexistente = contas.entrar(QStringLiteral("ninguem@escola.com"), senha);
        r.verificar(QStringLiteral("senha errada é recusada"), !errada.ok);
        r.verificar(QStringLiteral("e-mail inexistente dá a MESMA mensagem (não revela quem tem conta)"),
                    !inexistente.ok && inexistente.erro == errada.erro, errada.erro + " | " + inexistente.erro);

        // Bloqueio: 5 erros seguidos bloqueiam, mesmo com a senha certa, até o tempo passar
        // (a 1ª falha acima já contou; mais 3 aqui e a 5ª abaixo).
        ResultadoConta ultima;
        for (int i = 0; i < 3; ++i)
            ultima = contas.entrar(QStringLiteral("ana@escola.com"), QStringLiteral("tentativa%1x").arg(i));
        r.verificar(QStringLiteral("4ª senha errada ainda não bloqueia"), !ultima.ok && ultima.segundosDeBloqueio == 0);
        ultima = contas.entrar(QStringLiteral("ana@escola.com"), QStringLiteral("tentativa-5x"));
        r.verificar(QStringLiteral("5ª senha errada bloqueia por 30 s"), !ultima.ok && ultima.segundosDeBloqueio == 30, QString::number(ultima.segundosDeBloqueio));
        const ResultadoConta bloqueada = contas.entrar(QStringLiteral("ana@escola.com"), senha);
        r.verificar(QStringLiteral("conta bloqueada recusa até a senha certa"), !bloqueada.ok && bloqueada.segundosDeBloqueio > 0);
        agoraDeTeste += 31;
        r.verificar(QStringLiteral("depois do tempo do bloqueio, a senha certa entra de novo"), contas.entrar(QStringLiteral("ana@escola.com"), senha).ok);

        // A senha nunca é gravada em texto: o banco só tem hash e sal.
        {
            ContaRepository leitura(QDir(pastaContas).filePath(QStringLiteral("contas.db")));
            const auto registro = leitura.porEmail(QStringLiteral("ana@escola.com"));
            r.verificar(QStringLiteral("banco guarda só hash (64 bytes), sal (16 bytes) e 210.000 iterações"),
                        registro && registro->hash.size() == 64 && registro->sal.size() == 16 &&
                            registro->iteracoes == ContaService::kIteracoes && !registro->hash.contains(senha.toUtf8()) &&
                            registro->hash == ContaService::pbkdf2(senha.toUtf8(), registro->sal, registro->iteracoes));
        }

        // Recuperação de senha pelo código
        const QString novaSenha = QStringLiteral("Quadro!2030a");
        r.verificar(QStringLiteral("recuperar: código errado é recusado"),
                    !contas.redefinirSenha(QStringLiteral("ana@escola.com"), QStringLiteral("AAAA-BBBB-CCCC-DDDD-EEEE"), novaSenha).ok);
        const QString codigoAntigo = ana.codigoRecuperacao;
        const ResultadoConta fraca = contas.redefinirSenha(QStringLiteral("ana@escola.com"), codigoAntigo, QStringLiteral("12345678"));
        r.verificar(QStringLiteral("recuperar: senha nova fraca é recusada (e o código continua valendo)"), !fraca.ok);
        // O código vale em minúsculas e sem hífens (digitação mais livre)
        QString digitado = codigoAntigo.toLower();
        digitado.remove(QLatin1Char('-'));
        const ResultadoConta redefinida = contas.redefinirSenha(QStringLiteral("ana@escola.com"), digitado, novaSenha);
        r.verificar(QStringLiteral("recuperar: código certo (digitado em minúsculas, sem hífens) troca a senha"), redefinida.ok, redefinida.erro);
        r.verificar(QStringLiteral("recuperar: gera um código NOVO"), redefinida.ok && !redefinida.codigoRecuperacao.isEmpty() &&
                                                                      redefinida.codigoRecuperacao != codigoAntigo);
        r.verificar(QStringLiteral("recuperar: a senha antiga deixa de valer"), !contas.entrar(QStringLiteral("ana@escola.com"), senha).ok);
        r.verificar(QStringLiteral("recuperar: a senha nova entra"), contas.entrar(QStringLiteral("ana@escola.com"), novaSenha).ok);
        r.verificar(QStringLiteral("recuperar: o código antigo deixa de valer"),
                    !contas.redefinirSenha(QStringLiteral("ana@escola.com"), codigoAntigo, QStringLiteral("Outra!2031bb")).ok);
    }
    ContaService::usarRelogioDeTeste(nullptr);
}

// ---------------------------------------------------------------------------
// Segurança: texto do usuário não pode virar fórmula do Excel; tipos perigosos de anexo são recusados.
// ---------------------------------------------------------------------------
void testeSeguranca(Relatorio &r, const QString &pasta)
{
    r.titulo(QStringLiteral("Segurança (injeção de fórmula, anexos perigosos)"));

    XlsxService::DadosExportacao dados;
    dados.titulo = QStringLiteral("Teste");
    Aluno a;
    a.id = 1;
    a.matricula = QStringLiteral("=1+1");
    a.nome = QStringLiteral("=HYPERLINK(\"http://exemplo.invalido\",\"clique\")");
    dados.alunos.append(a);
    Avaliacao av;
    av.id = 1;
    av.nome = QStringLiteral("+SUM(A1:A2)");
    dados.avaliacoes.append(av);

    const QString arquivo = pasta + QStringLiteral("/injecao.xlsx");
    QString erro;
    const bool exportou = XlsxService::exportar(arquivo, dados, &erro);
    r.verificar(QStringLiteral("exportar planilha de teste"), exportou, erro);
    if (exportou) {
        QXlsx::Document doc(arquivo);
        const bool carregou = doc.load();
        const auto nome = doc.cellAt(5, 2);
        const auto matricula = doc.cellAt(5, 1);
        r.verificar(QStringLiteral("nome do aluno começando com \"=\" é gravado como TEXTO, não como fórmula"),
                    carregou && nome && !nome->hasFormula() && doc.read(5, 2).toString() == a.nome,
                    doc.read(5, 2).toString());
        r.verificar(QStringLiteral("matrícula começando com \"=\" é gravada como TEXTO, não como fórmula"),
                    carregou && matricula && !matricula->hasFormula() && doc.read(5, 1).toString() == a.matricula);
        const auto media = doc.cellAt(5, 4);
        r.verificar(QStringLiteral("a coluna Média continua sendo fórmula (gerada pelo programa)"), carregou && media && media->hasFormula());
    }

    r.verificar(QStringLiteral("anexo: executáveis e scripts são recusados"),
                ehArquivoExecutavel(QStringLiteral("C:/x/programa.EXE")) && ehArquivoExecutavel(QStringLiteral("a.ps1")) &&
                    ehArquivoExecutavel(QStringLiteral("relatorio.pdf.exe")));
    r.verificar(QStringLiteral("anexo: documentos do Office com macros e imagens de disco são recusados"),
                ehArquivoExecutavel(QStringLiteral("planilha.xlsm")) && ehArquivoExecutavel(QStringLiteral("aula.docm")) &&
                    ehArquivoExecutavel(QStringLiteral("jogo.iso")));
    r.verificar(QStringLiteral("anexo: PDF, apresentação e planilha comuns continuam permitidos"),
                !ehArquivoExecutavel(QStringLiteral("aula.pdf")) && !ehArquivoExecutavel(QStringLiteral("slides.pptx")) &&
                    !ehArquivoExecutavel(QStringLiteral("notas.xlsx")) && !ehArquivoExecutavel(QStringLiteral("foto.png")));
}

// ---------------------------------------------------------------------------
// IA (Cloudflare Workers AI) e verificação de atualização, contra um servidor HTTP FALSO local (127.0.0.1):
// confere o pedido que sai (caminho, token, corpo) e as respostas (fluxo SSE, JSON inteiro, erros, cancelamento,
// redirecionamento). Não usa a internet nem nenhuma chave de verdade.
// ---------------------------------------------------------------------------
class ServidorFalso : public QObject {
public:
    struct Resposta {
        int status = 200;
        QByteArray tipo = "text/event-stream";
        QList<QByteArray> partes;   // enviadas com um pequeno intervalo entre elas (testa linhas SSE cortadas ao meio)
        QByteArray cabecalhoExtra;  // ex.: "Location: http://..."
        bool naoResponder = false;  // mantém a conexão aberta (para testar cancelamento)
    };

    ServidorFalso()
    {
        m_servidor.listen(QHostAddress::LocalHost, 0);
        connect(&m_servidor, &QTcpServer::newConnection, this, &ServidorFalso::novaConexao);
    }

    quint16 porta() const { return m_servidor.serverPort(); }
    QString urlBase() const { return QStringLiteral("http://127.0.0.1:%1/client/v4").arg(porta()); }
    QString urlDireta() const { return QStringLiteral("http://127.0.0.1:%1/repos/x/releases/latest").arg(porta()); }

    Resposta resposta;
    QByteArray ultimoPedido;  // cabeçalhos + corpo do último pedido
    int conexoes = 0;

private:
    void novaConexao()
    {
        while (QTcpSocket *s = m_servidor.nextPendingConnection()) {
            ++conexoes;
            auto *buffer = new QByteArray;
            connect(s, &QTcpSocket::readyRead, this, [this, s, buffer] {
                *buffer += s->readAll();
                const int fim = buffer->indexOf("\r\n\r\n");
                if (fim < 0)
                    return;
                int tamanho = 0;
                const int c = buffer->toLower().indexOf("content-length:");
                if (c >= 0)
                    tamanho = buffer->mid(c + 15, buffer->indexOf("\r\n", c) - c - 15).trimmed().toInt();
                if (buffer->size() < fim + 4 + tamanho)
                    return;  // corpo ainda incompleto
                ultimoPedido = *buffer;
                buffer->clear();
                responder(s);
            });
            connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
        }
    }

    void responder(QTcpSocket *s)
    {
        if (resposta.naoResponder)
            return;
        QByteArray cab = "HTTP/1.1 " + QByteArray::number(resposta.status) + " X\r\nContent-Type: " + resposta.tipo +
                         "\r\nConnection: close\r\n";
        if (!resposta.cabecalhoExtra.isEmpty())
            cab += resposta.cabecalhoExtra + "\r\n";
        cab += "\r\n";
        s->write(cab);
        s->flush();
        const QList<QByteArray> partes = resposta.partes;
        QPointer<QTcpSocket> p(s);
        for (int i = 0; i < partes.size(); ++i) {
            QTimer::singleShot(40 * (i + 1), s, [p, parte = partes.at(i)] {
                if (p) {
                    p->write(parte);
                    p->flush();
                }
            });
        }
        QTimer::singleShot(40 * (partes.size() + 1), s, [p] {
            if (p)
                p->disconnectFromHost();
        });
    }

    QTcpServer m_servidor;
};

ResultadoIa pedirIa(IaService &ia, const ConfigIa &config, int limiteMs = 8000, int cancelarEmMs = -1)
{
    QEventLoop laco;
    ResultadoIa resultado;
    bool recebeu = false;
    QObject::connect(&ia, &IaService::concluido, &laco, [&](const ResultadoIa &r) {
        resultado = r;
        recebeu = true;
        laco.quit();
    });
    QTimer::singleShot(limiteMs, &laco, &QEventLoop::quit);
    if (cancelarEmMs >= 0)
        QTimer::singleShot(cancelarEmMs, &ia, &IaService::cancelar);
    ia.enviar(config, {{QStringLiteral("system"), QStringLiteral("Seja breve.")}, {QStringLiteral("user"), QStringLiteral("Diga olá")}}, 64);
    if (!recebeu)  // (uma configuração inválida já responde dentro de enviar())
        laco.exec();
    if (!recebeu)
        resultado.erro = QStringLiteral("(tempo esgotado no teste)");
    return resultado;
}

void testeIaEAtualizacao(Relatorio &r)
{
    r.titulo(QStringLiteral("IA (Cloudflare Workers AI), atualização e segredos"));

    const QString conta = QStringLiteral("0123456789abcdef0123456789abcdef");
    const QString token = QStringLiteral("TOKEN_de-teste_1234567890abcdefXYZ");
    ConfigIa config;
    config.accountId = conta;
    config.token = token;
    config.modelo = IaService::modeloPadrao();

    // --- Validação (nada de injeção em cabeçalho nem no caminho da URL) ---
    r.verificar(QStringLiteral("IA: Account ID, token e modelo válidos são aceitos"),
                IaService::accountIdValido(conta) && IaService::tokenValido(token) && IaService::modeloValido(IaService::modeloPadrao()) &&
                    config.completa());
    r.verificar(QStringLiteral("IA: Account ID com tamanho ou caracteres errados é recusado"),
                !IaService::accountIdValido(QStringLiteral("123")) && !IaService::accountIdValido(conta + QLatin1Char('0')) &&
                    !IaService::accountIdValido(QStringLiteral("zzzz456789abcdef0123456789abcdef")));
    r.verificar(QStringLiteral("IA: token com quebra de linha, espaço ou curto demais é recusado (anti injeção de cabeçalho)"),
                !IaService::tokenValido(token + QStringLiteral("\r\nX-Evil: 1")) && !IaService::tokenValido(token + QLatin1Char(' ')) &&
                    !IaService::tokenValido(QStringLiteral("curto")) && !IaService::tokenValido(QString()));
    r.verificar(QStringLiteral("IA: modelo com \"..\", outro domínio ou espaço é recusado (anti desvio de caminho)"),
                !IaService::modeloValido(QStringLiteral("@cf/../admin")) && !IaService::modeloValido(QStringLiteral("http://x.com/y")) &&
                    !IaService::modeloValido(QStringLiteral("@cf/meta/llama 3")) && !IaService::modeloValido(QStringLiteral("meta/llama")) &&
                    !IaService::modeloValido(QStringLiteral("@cf/meta/llama?x=1")) &&
                    IaService::modeloValido(QStringLiteral("@cf/meta/llama-3.3-70b-instruct-fp8-fast")));
    r.verificar(QStringLiteral("IA: a URL de produção é HTTPS em api.cloudflare.com"),
                IaService::urlDaApi(conta, IaService::modeloPadrao()).toString().startsWith(
                    QStringLiteral("https://api.cloudflare.com/client/v4/accounts/%1/ai/run/@cf/meta/").arg(conta)) &&
                    !IaService::urlDaApi(QStringLiteral("x"), IaService::modeloPadrao()).isValid());
    r.verificar(QStringLiteral("IA: o servidor de teste só aceita 127.0.0.1 (nunca aponta o token para a internet)"), [&] {
        IaService::usarServidorDeTeste(QStringLiteral("http://evil.example.com/v4"));
        const bool recusou = IaService::urlDaApi(conta, IaService::modeloPadrao()).host() == QLatin1String("api.cloudflare.com");
        return recusou;
    }());

    // --- Corpo do pedido e leitura do fluxo ---
    {
        const QByteArray corpo = IaService::montarCorpo({{QStringLiteral("system"), QStringLiteral("s")}, {QStringLiteral("admin"), QStringLiteral("u")}}, 99999, true);
        const QJsonObject o = QJsonDocument::fromJson(corpo).object();
        const QJsonArray msgs = o.value(QStringLiteral("messages")).toArray();
        r.verificar(QStringLiteral("IA: corpo do pedido (mensagens, papel inválido vira \"user\", max_tokens limitado, stream)"),
                    msgs.size() == 2 && msgs.at(0).toObject().value(QStringLiteral("role")).toString() == QStringLiteral("system") &&
                        msgs.at(1).toObject().value(QStringLiteral("role")).toString() == QStringLiteral("user") &&
                        o.value(QStringLiteral("max_tokens")).toInt() == 4096 && o.value(QStringLiteral("stream")).toBool());
        r.verificar(QStringLiteral("IA: sem fluxo o corpo não leva \"stream\""),
                    !QJsonDocument::fromJson(IaService::montarCorpo({{QStringLiteral("user"), QStringLiteral("x")}}, 10, false)).object().contains(QStringLiteral("stream")));
    }
    r.verificar(QStringLiteral("IA: linhas do fluxo SSE (trecho, [DONE], evento vazio, lixo, formato estilo OpenAI)"),
                IaService::trechoDaLinhaSse("data: {\"response\":\"Ol\"}") == QStringLiteral("Ol") &&
                    !IaService::trechoDaLinhaSse("data: [DONE]").has_value() && !IaService::trechoDaLinhaSse(": comentario").has_value() &&
                    IaService::trechoDaLinhaSse("data: {\"p\":\"x\"}") == QString() &&
                    IaService::trechoDaLinhaSse("data: nao-e-json") == QString() &&
                    IaService::trechoDaLinhaSse("data: {\"choices\":[{\"delta\":{\"content\":\"oi\"}}]}") == QStringLiteral("oi"));
    r.verificar(QStringLiteral("IA: resposta inteira em JSON (result.response)"),
                IaService::textoDaResposta("{\"result\":{\"response\":\"Tudo\"},\"success\":true}") == QStringLiteral("Tudo") &&
                    IaService::textoDaResposta("lixo").isEmpty());
    r.verificar(QStringLiteral("IA: mensagens de erro úteis (token, limite diário, sem internet) e sem expor o token"),
                IaService::mensagemDeErro(401, "{\"errors\":[{\"message\":\"Authentication error\"}]}", QNetworkReply::AuthenticationRequiredError).contains(QStringLiteral("token")) &&
                    IaService::mensagemDeErro(429, QByteArray(), QNetworkReply::UnknownContentError).contains(QStringLiteral("limite")) &&
                    IaService::mensagemDeErro(0, QByteArray(), QNetworkReply::HostNotFoundError).contains(QStringLiteral("internet")) &&
                    IaService::mensagemDeErro(503, QByteArray(), QNetworkReply::UnknownContentError).contains(QStringLiteral("problemas")) &&
                    !IaService::mensagemDeErro(401, QByteArray(), QNetworkReply::AuthenticationRequiredError).contains(token));

    // --- Contra o servidor falso ---
    ServidorFalso servidor;
    r.verificar(QStringLiteral("servidor falso local iniciado"), servidor.porta() != 0);
    IaService::usarServidorDeTeste(servidor.urlBase());
    IaService ia;

    // 1) fluxo SSE com linhas cortadas ao meio entre os pedaços
    {
        servidor.resposta = ServidorFalso::Resposta();
        servidor.resposta.partes = {QByteArray("data: {\"response\":\"Ol\"}\n\ndata: {\"res"), QByteArray("ponse\":\"\xC3\xA1!\"}\n\ndata: [DONE]\n\n")};
        QString acumulado;
        const QMetaObject::Connection ligacao = QObject::connect(&ia, &IaService::trecho, &ia, [&acumulado](const QString &t) { acumulado += t; });
        const ResultadoIa res = pedirIa(ia, config);
        r.verificar(QStringLiteral("IA (servidor falso): fluxo SSE com linha cortada ao meio chega inteiro (\"Olá!\")"),
                    res.ok && res.texto == QStringLiteral("Olá!") && acumulado == QStringLiteral("Olá!"), res.erro + res.texto);
        const QByteArray pedido = servidor.ultimoPedido;
        const QByteArray corpo = pedido.mid(pedido.indexOf("\r\n\r\n") + 4);
        const QJsonObject o = QJsonDocument::fromJson(corpo).object();
        r.verificar(QStringLiteral("IA (servidor falso): o pedido leva o token no cabeçalho Authorization, no caminho certo, com stream e as mensagens"),
                    pedido.startsWith("POST /client/v4/accounts/" + conta.toLatin1() + "/ai/run/") &&
                        pedido.contains("llama-3.1-8b-instruct-fp8") &&
                        pedido.toLower().contains("authorization: bearer " + token.toLower().toLatin1()) && o.value(QStringLiteral("stream")).toBool() &&
                        o.value(QStringLiteral("messages")).toArray().size() == 2,
                    QString::fromLatin1(pedido.left(260)));
        r.verificar(QStringLiteral("IA (servidor falso): o token NÃO vai na URL nem no corpo"),
                    !pedido.left(pedido.indexOf("\r\n")).contains(token.toLatin1()) && !corpo.contains(token.toLatin1()));
        QObject::disconnect(ligacao);
    }
    // 2) resposta inteira em JSON (sem fluxo)
    {
        servidor.resposta = ServidorFalso::Resposta();
        servidor.resposta.tipo = "application/json";
        servidor.resposta.partes = {QByteArray("{\"result\":{\"response\":\"Texto inteiro\"},\"success\":true,\"errors\":[],\"messages\":[]}")};
        const ResultadoIa res = pedirIa(ia, config);
        r.verificar(QStringLiteral("IA (servidor falso): resposta inteira em JSON também funciona"), res.ok && res.texto == QStringLiteral("Texto inteiro"), res.erro);
    }
    // 3) erros HTTP
    {
        servidor.resposta = ServidorFalso::Resposta();
        servidor.resposta.status = 401;
        servidor.resposta.tipo = "application/json";
        servidor.resposta.partes = {QByteArray("{\"success\":false,\"errors\":[{\"code\":10000,\"message\":\"Authentication error\"}]}")};
        const ResultadoIa res = pedirIa(ia, config);
        r.verificar(QStringLiteral("IA (servidor falso): 401 vira uma mensagem sobre o token (com o detalhe do serviço)"),
                    !res.ok && res.statusHttp == 401 && res.erro.contains(QStringLiteral("token")) && res.erro.contains(QStringLiteral("Authentication error")), res.erro);
        servidor.resposta.status = 429;
        servidor.resposta.partes = {QByteArray("{\"errors\":[]}")};
        const ResultadoIa limite = pedirIa(ia, config);
        r.verificar(QStringLiteral("IA (servidor falso): 429 explica o limite diário"), !limite.ok && limite.erro.contains(QStringLiteral("limite")), limite.erro);
    }
    // 4) resposta vazia
    {
        servidor.resposta = ServidorFalso::Resposta();
        servidor.resposta.partes = {QByteArray("data: [DONE]\n\n")};
        const ResultadoIa res = pedirIa(ia, config);
        r.verificar(QStringLiteral("IA (servidor falso): resposta sem texto vira erro claro"), !res.ok && !res.erro.isEmpty(), res.erro);
    }
    // 5) redirecionamento NÃO é seguido (o token não pode ir para outro endereço)
    {
        ServidorFalso destino;
        servidor.resposta = ServidorFalso::Resposta();
        servidor.resposta.status = 302;
        servidor.resposta.tipo = "text/plain";
        servidor.resposta.cabecalhoExtra = "Location: http://127.0.0.1:" + QByteArray::number(destino.porta()) + "/roubar";
        const ResultadoIa res = pedirIa(ia, config);
        r.verificar(QStringLiteral("IA (servidor falso): redirecionamento não é seguido e o outro endereço não recebe o token"),
                    !res.ok && destino.conexoes == 0 && destino.ultimoPedido.isEmpty(), res.erro);
    }
    // 6) cancelamento
    {
        servidor.resposta = ServidorFalso::Resposta();
        servidor.resposta.naoResponder = true;
        const ResultadoIa res = pedirIa(ia, config, 6000, /*cancelarEmMs=*/250);
        r.verificar(QStringLiteral("IA (servidor falso): cancelar interrompe o pedido"), res.cancelado && !res.ok && !ia.ocupado());
    }
    // 7) configuração inválida nem chega à rede
    {
        ConfigIa ruim = config;
        ruim.token = QStringLiteral("abc\r\nHost: evil");
        const int antes = servidor.conexoes;
        const ResultadoIa res = pedirIa(ia, ruim, 3000);
        r.verificar(QStringLiteral("IA: configuração inválida é recusada antes de abrir qualquer conexão"),
                    !res.ok && !res.erro.isEmpty() && servidor.conexoes == antes, res.erro);
    }
    IaService::usarServidorDeTeste(QString());

    // --- Verificação de atualização ---
    r.verificar(QStringLiteral("Atualização: só aceita URL de Releases do repositório (https, github.com, sem usuário nem porta)"),
                AtualizacaoService::urlConfiavel(QStringLiteral("https://github.com/novakzx/Caderno-/releases/tag/v1.2.0")) &&
                    AtualizacaoService::urlConfiavel(QStringLiteral("https://github.com/novakzx/Caderno-/releases")) &&
                    !AtualizacaoService::urlConfiavel(QStringLiteral("http://github.com/novakzx/Caderno-/releases/tag/v1")) &&
                    !AtualizacaoService::urlConfiavel(QStringLiteral("https://github.com.evil.com/novakzx/Caderno-/releases/tag/v1")) &&
                    !AtualizacaoService::urlConfiavel(QStringLiteral("https://github.com/outro/repo/releases/tag/v1")) &&
                    !AtualizacaoService::urlConfiavel(QStringLiteral("https://usuario@github.com/novakzx/Caderno-/releases/tag/v1")) &&
                    !AtualizacaoService::urlConfiavel(QStringLiteral("https://github.com:444/novakzx/Caderno-/releases/tag/v1")) &&
                    !AtualizacaoService::urlConfiavel(QStringLiteral("https://github.com/novakzx/Caderno-/releases/../../x")) &&
                    !AtualizacaoService::urlConfiavel(QStringLiteral("javascript:alert(1)")));
    {
        const auto nova = AtualizacaoService::interpretar(
            "{\"tag_name\":\"v9.9.9\",\"html_url\":\"https://github.com/novakzx/Caderno-/releases/tag/v9.9.9\"}", QStringLiteral("1.1.0"));
        r.verificar(QStringLiteral("Atualização: versão mais nova é detectada, com o link da Release"),
                    nova.ok && nova.temNova && nova.versao == QStringLiteral("9.9.9") && nova.url.endsWith(QStringLiteral("/tag/v9.9.9")));
        const auto fraude = AtualizacaoService::interpretar("{\"tag_name\":\"v9.9.9\",\"html_url\":\"https://evil.example/baixar.exe\"}", QStringLiteral("1.1.0"));
        r.verificar(QStringLiteral("Atualização: link de fora é trocado pela página oficial de Releases"),
                    fraude.ok && fraude.url == AtualizacaoService::paginaDeReleases());
        const auto igual = AtualizacaoService::interpretar("{\"tag_name\":\"v1.1.0\",\"html_url\":\"\"}", QStringLiteral("1.1.0"));
        const auto lixo = AtualizacaoService::interpretar("{\"tag_name\":\"nightly\"}", QStringLiteral("1.1.0"));
        r.verificar(QStringLiteral("Atualização: mesma versão não é \"nova\"; etiqueta ilegível e JSON inválido dão erro"),
                    igual.ok && !igual.temNova && !lixo.ok && !lixo.erro.isEmpty() && !AtualizacaoService::interpretar("<html>", QStringLiteral("1.1.0")).ok);
    }
    {
        ServidorFalso github;
        github.resposta.tipo = "application/json";
        github.resposta.partes = {QByteArray("{\"tag_name\":\"v9.9.9\",\"html_url\":\"https://github.com/novakzx/Caderno-/releases/tag/v9.9.9\"}")};
        AtualizacaoService::usarServidorDeTeste(github.urlDireta());
        AtualizacaoService servico;
        QEventLoop laco;
        ResultadoAtualizacao res;
        QObject::connect(&servico, &AtualizacaoService::concluido, &laco, [&](const ResultadoAtualizacao &x) {
            res = x;
            laco.quit();
        });
        QTimer::singleShot(6000, &laco, &QEventLoop::quit);
        servico.verificar(QStringLiteral("1.1.0"));
        laco.exec();
        r.verificar(QStringLiteral("Atualização (servidor falso): consulta, interpreta e não envia dados do usuário"),
                    res.ok && res.temNova && !github.ultimoPedido.toLower().contains("authorization") && github.ultimoPedido.toLower().contains("user-agent: caderno+/") &&
                        !github.ultimoPedido.toLower().contains("cookie"),
                    res.erro + QString::fromLatin1(github.ultimoPedido.left(300)));

        github.resposta.status = 404;
        QEventLoop laco2;
        ResultadoAtualizacao semRelease;
        QObject::connect(&servico, &AtualizacaoService::concluido, &laco2, [&](const ResultadoAtualizacao &x) {
            semRelease = x;
            laco2.quit();
        });
        QTimer::singleShot(6000, &laco2, &QEventLoop::quit);
        servico.verificar(QStringLiteral("1.1.0"));
        laco2.exec();
        r.verificar(QStringLiteral("Atualização (servidor falso): 404 (nenhuma Release ainda) vira mensagem, sem travar"), !semRelease.ok && !semRelease.erro.isEmpty());
        AtualizacaoService::usarServidorDeTeste(QString());
    }

    // --- Proteção da chave (DPAPI do Windows) ---
    if (SegredoService::disponivel()) {
        const QString segredo = QStringLiteral("chave-secreta-ü-1234567890");
        const QString protegido = SegredoService::proteger(segredo);
        QString adulterado = protegido;
        adulterado[adulterado.size() / 2] = (adulterado[adulterado.size() / 2] == QLatin1Char('A')) ? QLatin1Char('B') : QLatin1Char('A');
        r.verificar(QStringLiteral("Segredo (DPAPI): vai e volta, e o texto protegido não contém a chave"),
                    !protegido.isEmpty() && !protegido.contains(QStringLiteral("secreta")) && SegredoService::revelar(protegido) == segredo);
        r.verificar(QStringLiteral("Segredo (DPAPI): texto adulterado ou inventado não abre"),
                    SegredoService::revelar(adulterado).isEmpty() && SegredoService::revelar(QStringLiteral("bGl4by1pbnZhbGlkbw==")).isEmpty() &&
                        SegredoService::revelar(QString()).isEmpty() && SegredoService::proteger(QString()).isEmpty());
    } else {
        r.info(QStringLiteral("proteção de segredos indisponível neste sistema (só Windows): teste ignorado"));
    }

    // --- Pedidos para a IA (prompts) ---
    {
        IaPrompts::Pedido p;
        p.tarefa = IaPrompts::Tarefa::PlanoDeAula;
        p.disciplina = QStringLiteral("Matemática");
        p.serie = QStringLiteral("8º ano");
        p.tema = QStringLiteral("Equações do 1º grau");
        p.duracao = QStringLiteral("50 minutos");
        p.detalhes = QStringLiteral("sem projetor");
        const auto m = IaPrompts::montar(p);
        r.verificar(QStringLiteral("Prompts: o plano de aula leva só o que o professor preencheu, em português, com aviso de não usar dados de alunos"),
                    m.size() == 2 && m.at(0).papel == QStringLiteral("system") && m.at(0).conteudo.contains(QStringLiteral("português")) &&
                        m.at(0).conteudo.contains(QStringLiteral("dados pessoais")) && m.at(1).conteudo.contains(QStringLiteral("Equações do 1º grau")) &&
                        m.at(1).conteudo.contains(QStringLiteral("Matemática")) && m.at(1).conteudo.contains(QStringLiteral("sem projetor")));
        IaPrompts::Pedido livre;
        livre.tarefa = IaPrompts::Tarefa::Livre;
        livre.detalhes = QStringLiteral("Explique fotossíntese") + QChar(0) + QLatin1Char(' ') + QChar(0x202e) + QStringLiteral(" em 3 linhas");
        const QString texto = IaPrompts::montar(livre).at(1).conteudo;
        r.verificar(QStringLiteral("Prompts: caracteres de controle e invisíveis são removidos do que vai para o serviço"),
                    texto.contains(QStringLiteral("Explique fotossíntese")) && !texto.contains(QChar(0)) && !texto.contains(QChar(0x202e)));
        r.verificar(QStringLiteral("Prompts: texto enorme é cortado no limite e pedido vazio ganha um texto padrão"),
                    IaPrompts::limpar(QString(10000, QLatin1Char('a')), 200).size() == 200 &&
                        !IaPrompts::montar(IaPrompts::Pedido{IaPrompts::Tarefa::Livre, {}, {}, {}, {}, {}, 5}).at(1).conteudo.isEmpty());
    }
}

}  // namespace

int executar(const QString &arquivoSaida)
{
    Relatorio r(arquivoSaida);
    r.linha(QStringLiteral("Autoteste do Caderno+"));
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

        testeMigracaoDeCores(r, pasta.path());
        testeRecursosDoDesign(r);
        testeContas(r, pasta.path());
        testeSeguranca(r, pasta.path());
        testeIaEAtualizacao(r);

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
