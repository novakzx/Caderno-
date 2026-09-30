#include "database/Migrations.h"

#include <QSqlError>
#include <QSqlQuery>

namespace Migrations {

// ---------------------------------------------------------------------------
// Migração 1: esquema completo previsto para TODAS as etapas do projeto.
// As telas de notas, horário, aulas, anotações etc. ainda não existem, mas as
// tabelas já estão prontas para que as próximas etapas não precisem migrar.
//
// Convenções:
//  - datas em texto ISO ("2026-09-30"), horas como "HH:mm", datetime ISO;
//  - dia_semana: 1 = segunda ... 7 = domingo (igual a Qt::DayOfWeek);
//  - chaves estrangeiras com ON DELETE CASCADE (apagar a turma apaga alunos,
//    notas, frequência...) ou SET NULL quando o vínculo é opcional.
// ---------------------------------------------------------------------------
static Migracao migracaoInicial()
{
    Migracao m;
    m.versao = 1;
    m.descricao = QStringLiteral("Esquema inicial completo");
    m.comandos = {
        // --- Turmas e alunos (Etapa 1) ---
        R"(CREATE TABLE turmas (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            nome        TEXT    NOT NULL,
            disciplina  TEXT    NOT NULL DEFAULT '',
            ano_letivo  INTEGER NOT NULL,
            periodo     TEXT    NOT NULL DEFAULT '',
            sala        TEXT    NOT NULL DEFAULT '',
            cor         TEXT    NOT NULL DEFAULT '#4C8BF5',
            arquivada   INTEGER NOT NULL DEFAULT 0,
            criada_em   TEXT    NOT NULL DEFAULT (datetime('now'))
        ))",

        R"(CREATE TABLE alunos (
            id               INTEGER PRIMARY KEY AUTOINCREMENT,
            turma_id         INTEGER NOT NULL REFERENCES turmas(id) ON DELETE CASCADE,
            nome             TEXT    NOT NULL,
            matricula        TEXT    NOT NULL DEFAULT '',
            email            TEXT    NOT NULL DEFAULT '',
            data_nascimento  TEXT,
            observacoes      TEXT    NOT NULL DEFAULT '',
            ativo            INTEGER NOT NULL DEFAULT 1,
            criado_em        TEXT    NOT NULL DEFAULT (datetime('now'))
        ))",
        "CREATE INDEX idx_alunos_turma ON alunos(turma_id)",
        // Matrícula única dentro da turma, mas só quando preenchida.
        "CREATE UNIQUE INDEX idx_alunos_matricula ON alunos(turma_id, matricula) "
        "WHERE matricula <> ''",

        // --- Notas (Etapa 2) ---
        // Cada linha = uma coluna da planilha (prova, trabalho...), com peso.
        R"(CREATE TABLE avaliacoes (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            turma_id     INTEGER NOT NULL REFERENCES turmas(id) ON DELETE CASCADE,
            nome         TEXT    NOT NULL,
            tipo         TEXT    NOT NULL DEFAULT 'prova',
            peso         REAL    NOT NULL DEFAULT 1.0,
            nota_maxima  REAL    NOT NULL DEFAULT 10.0,
            data         TEXT,
            periodo      INTEGER NOT NULL DEFAULT 1,   -- bimestre/trimestre
            ordem        INTEGER NOT NULL DEFAULT 0
        ))",
        "CREATE INDEX idx_avaliacoes_turma ON avaliacoes(turma_id)",

        // Cada linha = célula da planilha (aluno x avaliação). valor NULL = sem nota.
        R"(CREATE TABLE notas (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            avaliacao_id  INTEGER NOT NULL REFERENCES avaliacoes(id) ON DELETE CASCADE,
            aluno_id      INTEGER NOT NULL REFERENCES alunos(id)     ON DELETE CASCADE,
            valor         REAL,
            UNIQUE (avaliacao_id, aluno_id)
        ))",

        // --- Horário semanal (Etapa 3) ---
        R"(CREATE TABLE horarios (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            turma_id     INTEGER NOT NULL REFERENCES turmas(id) ON DELETE CASCADE,
            dia_semana   INTEGER NOT NULL CHECK (dia_semana BETWEEN 1 AND 7),
            hora_inicio  TEXT    NOT NULL,
            hora_fim     TEXT    NOT NULL,
            sala         TEXT    NOT NULL DEFAULT ''
        ))",
        "CREATE INDEX idx_horarios_turma ON horarios(turma_id)",

        // --- Aulas / plano de aula (Etapa 3-4) ---
        R"(CREATE TABLE aulas (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            turma_id     INTEGER NOT NULL REFERENCES turmas(id) ON DELETE CASCADE,
            data         TEXT    NOT NULL,
            tema         TEXT    NOT NULL DEFAULT '',
            objetivos    TEXT    NOT NULL DEFAULT '',
            materiais    TEXT    NOT NULL DEFAULT '',
            observacoes  TEXT    NOT NULL DEFAULT ''
        ))",
        "CREATE INDEX idx_aulas_turma_data ON aulas(turma_id, data)",

        // --- Arquivos e apresentações (Etapa 4) ---
        // Guardamos só o caminho; o arquivo abre no programa padrão do sistema.
        R"(CREATE TABLE anexos (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            turma_id   INTEGER REFERENCES turmas(id) ON DELETE CASCADE,
            aula_id    INTEGER REFERENCES aulas(id)  ON DELETE CASCADE,
            aluno_id   INTEGER REFERENCES alunos(id) ON DELETE CASCADE,
            nome       TEXT NOT NULL,
            caminho    TEXT NOT NULL,
            tipo       TEXT NOT NULL DEFAULT '',
            criado_em  TEXT NOT NULL DEFAULT (datetime('now'))
        ))",

        // --- Anotações com tags (Etapa 4) ---
        R"(CREATE TABLE anotacoes (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            titulo          TEXT NOT NULL DEFAULT '',
            conteudo_html   TEXT NOT NULL DEFAULT '',
            turma_id        INTEGER REFERENCES turmas(id) ON DELETE SET NULL,
            aluno_id        INTEGER REFERENCES alunos(id) ON DELETE SET NULL,
            aula_id         INTEGER REFERENCES aulas(id)  ON DELETE SET NULL,
            criada_em       TEXT NOT NULL DEFAULT (datetime('now')),
            atualizada_em   TEXT NOT NULL DEFAULT (datetime('now'))
        ))",
        R"(CREATE TABLE tags (
            id    INTEGER PRIMARY KEY AUTOINCREMENT,
            nome  TEXT NOT NULL UNIQUE COLLATE NOCASE
        ))",
        R"(CREATE TABLE anotacao_tags (
            anotacao_id  INTEGER NOT NULL REFERENCES anotacoes(id) ON DELETE CASCADE,
            tag_id       INTEGER NOT NULL REFERENCES tags(id)      ON DELETE CASCADE,
            PRIMARY KEY (anotacao_id, tag_id)
        ))",

        // --- Frequência / chamada (Etapa 5) ---
        // situacao: 'P' presente, 'F' falta, 'J' falta justificada, 'A' atraso
        R"(CREATE TABLE frequencia (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            aluno_id      INTEGER NOT NULL REFERENCES alunos(id) ON DELETE CASCADE,
            data          TEXT    NOT NULL,
            situacao      TEXT    NOT NULL DEFAULT 'P' CHECK (situacao IN ('P','F','J','A')),
            justificativa TEXT    NOT NULL DEFAULT '',
            UNIQUE (aluno_id, data)
        ))",

        // --- Tarefas (Etapa 5) ---
        R"(CREATE TABLE tarefas (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            turma_id      INTEGER REFERENCES turmas(id) ON DELETE SET NULL,
            titulo        TEXT    NOT NULL,
            descricao     TEXT    NOT NULL DEFAULT '',
            data_entrega  TEXT,
            concluida     INTEGER NOT NULL DEFAULT 0,
            prioridade    INTEGER NOT NULL DEFAULT 1    -- 0 baixa, 1 normal, 2 alta
        ))",

        // --- Calendário escolar (Etapa 5): provas, feriados, reuniões ---
        R"(CREATE TABLE eventos (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            turma_id     INTEGER REFERENCES turmas(id) ON DELETE CASCADE,
            titulo       TEXT NOT NULL,
            tipo         TEXT NOT NULL DEFAULT 'evento',  -- prova, feriado, reuniao, evento
            data_inicio  TEXT NOT NULL,
            data_fim     TEXT,
            descricao    TEXT NOT NULL DEFAULT ''
        ))",
        "CREATE INDEX idx_eventos_data ON eventos(data_inicio)",

        // --- Preferências simples chave/valor (extras futuros) ---
        R"(CREATE TABLE configuracoes (
            chave  TEXT PRIMARY KEY,
            valor  TEXT NOT NULL
        ))",
    };
    return m;
}

// ---------------------------------------------------------------------------
// Migração 2 (Etapa 2): índices para a planilha de notas e a busca de notas
// por aluno. Serve também de exemplo de como evoluir o esquema.
// ---------------------------------------------------------------------------
static Migracao migracaoIndicesNotas()
{
    Migracao m;
    m.versao = 2;
    m.descricao = QStringLiteral("Índices de notas");
    m.comandos = {
        "CREATE INDEX IF NOT EXISTS idx_notas_aluno ON notas(aluno_id)",
        "CREATE INDEX IF NOT EXISTS idx_avaliacoes_turma_periodo ON avaliacoes(turma_id, periodo, ordem)",
    };
    return m;
}

// ---------------------------------------------------------------------------
// Migração 3 (Etapa 3): índices para a grade semanal e o painel "Hoje"
// (aulas do dia, tarefas pendentes, provas próximas).
// ---------------------------------------------------------------------------
static Migracao migracaoIndicesHoje()
{
    Migracao m;
    m.versao = 3;
    m.descricao = QStringLiteral("Índices do horário e do painel Hoje");
    m.comandos = {
        "CREATE INDEX IF NOT EXISTS idx_horarios_dia ON horarios(dia_semana, hora_inicio)",
        "CREATE INDEX IF NOT EXISTS idx_tarefas_pendentes ON tarefas(concluida, data_entrega)",
        "CREATE INDEX IF NOT EXISTS idx_avaliacoes_data ON avaliacoes(data)",
    };
    return m;
}

// ---------------------------------------------------------------------------
// Migração 4 (Etapas 4 e 5): texto simples das anotações (para a busca) e
// índices de anotações, anexos e frequência.
// ---------------------------------------------------------------------------
static Migracao migracaoAnotacoesFrequencia()
{
    Migracao m;
    m.versao = 4;
    m.descricao = QStringLiteral("Texto das anotações e índices de anexos/frequência");
    m.comandos = {
        // Mesmo conteúdo da anotação sem formatação, gravado junto com o HTML.
        "ALTER TABLE anotacoes ADD COLUMN conteudo_texto TEXT NOT NULL DEFAULT ''",
        "CREATE INDEX IF NOT EXISTS idx_anotacoes_turma ON anotacoes(turma_id)",
        "CREATE INDEX IF NOT EXISTS idx_anotacoes_aluno ON anotacoes(aluno_id)",
        "CREATE INDEX IF NOT EXISTS idx_anexos_turma ON anexos(turma_id)",
        "CREATE INDEX IF NOT EXISTS idx_anexos_aula ON anexos(aula_id)",
        "CREATE INDEX IF NOT EXISTS idx_frequencia_data ON frequencia(data)",
    };
    return m;
}

// ---------------------------------------------------------------------------
// Migração 5 (design system do Caderno+): a cor da turma passa a ser guardada como
// NOME de token ("turma-1" a "turma-6"), que acompanha o tema claro/escuro. O azul
// antigo (#4C8BF5), que era o padrão de turma nova, vira "turma-6" (lousa).
//
// Cores escolhidas à mão ("#rrggbb") continuam valendo como estão.
// O DEFAULT da coluna (migração 1) não pode ser alterado sem recriar a tabela, mas
// não é mais usado: TurmaRepository sempre grava a cor, e o padrão está em Turma::cor.
// ---------------------------------------------------------------------------
static Migracao migracaoCoresDeTurma()
{
    Migracao m;
    m.versao = 5;
    m.descricao = QStringLiteral("Cor da turma como token do design system");
    m.comandos = {
        "UPDATE turmas SET cor = 'turma-6' WHERE upper(cor) = '#4C8BF5'",
    };
    return m;
}

const QList<Migracao> &todas()
{
    // Para evoluir o esquema, acrescente novas migrações AQUI, no fim
    // (versao = última + 1).
    static const QList<Migracao> lista = {
        migracaoInicial(),
        migracaoIndicesNotas(),
        migracaoIndicesHoje(),
        migracaoAnotacoesFrequencia(),
        migracaoCoresDeTurma(),
    };
    return lista;
}

// Executa UM comando SQL numa consulta própria e a encerra por completo.
//
// Por que tanto cuidado: o SQLite recusa o COMMIT ("cannot commit transaction -
// SQL statements in progress") se ainda houver um comando "em andamento" na
// conexão. Um comando que devolve linhas (SELECT, PRAGMA journal_mode...) só
// termina depois de lido até o fim ou "resetado". Aqui cada comando tem a sua
// QSqlQuery, que é lida até o fim, finalizada e destruída antes do próximo.
bool executarComando(QSqlDatabase &db, const QString &sql, QString *erro)
{
    QSqlQuery q(db);
    const bool ok = q.exec(sql);
    if (!ok) {
        if (erro)
            *erro = q.lastError().text();
        return false;
    }
    while (q.isSelect() && q.next()) {
        // descarta as linhas de resultado, se houver
    }
    q.finish();
    return true;
}

namespace {

// Texto curto para acompanhar mensagens de erro: ajuda a diagnosticar problemas
// que só acontecem em alguns computadores.
QString diagnostico(QSqlDatabase &db)
{
    QString versao = QStringLiteral("?"), modo = QStringLiteral("?");
    {
        QSqlQuery q(db);
        if (q.exec(QStringLiteral("SELECT sqlite_version()")) && q.next())
            versao = q.value(0).toString();
        q.finish();
    }
    {
        QSqlQuery q(db);
        if (q.exec(QStringLiteral("PRAGMA journal_mode")) && q.next())
            modo = q.value(0).toString();
        q.finish();
    }
    return QStringLiteral(" [SQLite %1, journal_mode=%2]").arg(versao, modo);
}

// Aplica UMA migração dentro de uma transação explícita (BEGIN ... COMMIT).
bool aplicarUma(QSqlDatabase &db, const Migracao &m, QString *erro)
{
    QString detalhe;
    if (!executarComando(db, QStringLiteral("BEGIN IMMEDIATE"), &detalhe)) {
        if (erro)
            *erro = QStringLiteral("Não foi possível iniciar a migração %1: %2").arg(m.versao).arg(detalhe);
        return false;
    }

    auto desfazer = [&](const QString &motivo) {
        executarComando(db, QStringLiteral("ROLLBACK"), nullptr);
        if (erro)
            *erro = motivo;
        return false;
    };

    for (const QString &sql : m.comandos) {
        if (!executarComando(db, sql, &detalhe))
            return desfazer(QStringLiteral("Migração %1 (%2) falhou: %3").arg(m.versao).arg(m.descricao, detalhe));
    }

    // PRAGMA user_version é transacional no SQLite: só vale se tudo deu certo.
    // (PRAGMA não aceita parâmetros, por isso o número é montado no texto;
    // ele vem do nosso código, não do usuário.)
    if (!executarComando(db, QStringLiteral("PRAGMA user_version = %1").arg(m.versao), &detalhe))
        return desfazer(QStringLiteral("Não foi possível gravar a versão %1: %2").arg(m.versao).arg(detalhe));

    if (!executarComando(db, QStringLiteral("COMMIT"), &detalhe))
        return desfazer(QStringLiteral("Não foi possível concluir a migração %1: %2%3")
                            .arg(m.versao)
                            .arg(detalhe, diagnostico(db)));
    return true;
}

}  // namespace

int versaoAtual(QSqlDatabase &db)
{
    QSqlQuery q(db);
    int versao = 0;
    if (q.exec(QStringLiteral("PRAGMA user_version")) && q.next())
        versao = q.value(0).toInt();
    q.finish();
    return versao;
}

bool aplicar(QSqlDatabase &db, QString *erro)
{
    const int atual = versaoAtual(db);

    for (const Migracao &m : todas()) {
        if (m.versao <= atual)
            continue;
        if (!aplicarUma(db, m, erro))
            return false;
    }
    return true;
}

}  // namespace Migrations
