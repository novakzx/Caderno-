#include "services/ImportadorAlunos.h"

#include "core/CsvUtil.h"
#include "core/TextoUtil.h"
#include "database/AlunoRepository.h"
#include "services/XlsxService.h"

#include <QDate>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QStringConverter>

namespace {

constexpr int kMaximoNome = 120;
constexpr int kMaximoMatricula = 40;
constexpr int kMaximoEmail = 254;
constexpr int kMaximoObservacoes = 1000;

// Título de coluna "canônico": sem acentos, sem pontuação, em minúsculas ("N.º de matrícula" -> "n de matricula").
QString canonico(const QString &titulo)
{
    QString s = normalizarTexto(titulo);
    for (QChar &c : s) {
        if (!c.isLetterOrNumber())
            c = QLatin1Char(' ');
    }
    return s.simplified();
}

enum class Coluna { Nome, Matricula, Email, Nascimento, Observacoes, Nenhuma };

Coluna classificar(const QString &titulo)
{
    static const QSet<QString> nome = {"nome", "aluno", "aluna", "alunos", "estudante", "nome do aluno", "nome da aluna",
                                       "nome do estudante", "nome completo"};
    static const QSet<QString> matricula = {"matricula", "mat", "matr", "ra", "registro", "registro do aluno",
                                            "numero de matricula", "n de matricula", "n matricula", "codigo", "cod"};
    static const QSet<QString> email = {"email", "e mail", "correio eletronico", "endereco de email", "email do aluno"};
    static const QSet<QString> nascimento = {"nascimento", "data de nascimento", "data nascimento", "nasc", "dt nascimento",
                                             "data de nasc", "aniversario"};
    static const QSet<QString> observacoes = {"observacoes", "observacao", "obs", "anotacoes"};

    const QString c = canonico(titulo);
    if (nome.contains(c)) return Coluna::Nome;
    if (matricula.contains(c)) return Coluna::Matricula;
    if (email.contains(c)) return Coluna::Email;
    if (nascimento.contains(c)) return Coluna::Nascimento;
    if (observacoes.contains(c)) return Coluna::Observacoes;
    return Coluna::Nenhuma;
}

QDate lerData(const QString &texto)
{
    static const char *formatos[] = {"d/M/yyyy", "d-M-yyyy", "d.M.yyyy", "yyyy-M-d"};
    const QString t = texto.trimmed().left(10);  // aceita "2012-03-15T00:00:00" também
    for (const char *formato : formatos) {
        const QDate d = QDate::fromString(t, QLatin1String(formato));
        if (d.isValid())
            return d;
    }
    return QDate();
}

bool emailPlausivel(const QString &email)
{
    static const QRegularExpression padrao(QStringLiteral(R"(^[^@\s]+@[^@\s]+\.[^@\s]+$)"));
    return email.size() <= kMaximoEmail && padrao.match(email).hasMatch();
}

QString celula(const QStringList &linha, int coluna)
{
    return (coluna >= 0 && coluna < linha.size()) ? linha.at(coluna) : QString();
}

// Registro de quem já "ocupa" uma matrícula/nome (alunos da turma, ou linhas já aceitas da lista).
// Dois alunos são o mesmo quando têm a mesma matrícula, ou o mesmo nome e uma das matrículas está vazia
// (homônimos com matrículas diferentes são pessoas diferentes).
struct Ocupados {
    QSet<QString> matriculas;
    QSet<QString> nomesSemMatricula;
    QSet<QString> nomesComMatricula;

    bool contem(const QString &nome, const QString &matricula) const
    {
        if (!matricula.isEmpty())
            return matriculas.contains(matricula) || nomesSemMatricula.contains(nome);
        return nomesSemMatricula.contains(nome) || nomesComMatricula.contains(nome);
    }
    void adicionar(const QString &nome, const QString &matricula)
    {
        if (matricula.isEmpty()) {
            nomesSemMatricula.insert(nome);
        } else {
            matriculas.insert(matricula);
            nomesComMatricula.insert(nome);
        }
    }
};

}  // namespace

std::optional<QList<QStringList>> ImportadorAlunos::tabelaDeTexto(const QByteArray &bytes, QString *erro)
{
    auto falha = [erro](const QString &msg) -> std::optional<QList<QStringList>> {
        if (erro)
            *erro = msg;
        return std::nullopt;
    };
    if (bytes.size() > kTamanhoMaximoDoTexto)
        return falha(QStringLiteral("O arquivo é grande demais (máximo de 5 MB)."));

    QByteArray dados = bytes;
    if (dados.startsWith("\xEF\xBB\xBF"))  // BOM do UTF-8 (o Excel o põe)
        dados.remove(0, 3);

    // UTF-8 quando for válido; senão Windows-1252/Latin-1 (o "CSV" antigo do Excel).
    QString texto;
    QStringDecoder utf8(QStringConverter::Utf8, QStringConverter::Flag::Stateless);
    texto = utf8(dados);
    if (utf8.hasError())
        texto = QString::fromLatin1(dados);

    const std::string bruto = texto.toUtf8().toStdString();
    const CsvUtil::Tabela tabela = CsvUtil::analisar(bruto, CsvUtil::detectarSeparador(bruto));

    QList<QStringList> resultado;
    for (const CsvUtil::Linha &linha : tabela) {
        if (linha.size() > 200)
            return falha(QStringLiteral("A lista tem colunas demais (máximo de 200)."));
        QStringList celulas;
        bool temConteudo = false;
        for (const std::string &campo : linha) {
            const QString c = QString::fromUtf8(campo.data(), static_cast<qsizetype>(campo.size())).trimmed();
            temConteudo = temConteudo || !c.isEmpty();
            celulas << c;
        }
        if (temConteudo)
            resultado.append(celulas);
        if (resultado.size() > kMaximoDeLinhas)
            return falha(QStringLiteral("A lista é grande demais (máximo de %1 linhas por importação).").arg(kMaximoDeLinhas));
    }
    if (resultado.isEmpty())
        return falha(QStringLiteral("Não encontrei nenhuma linha com conteúdo."));
    return resultado;
}

std::optional<QList<QStringList>> ImportadorAlunos::tabelaDeArquivo(const QString &caminho, QString *erro)
{
    const QString extensao = QFileInfo(caminho).suffix().toLower();
    if (extensao == QLatin1String("xlsx")) {
        auto tabela = XlsxService::lerTabela(caminho, erro);
        if (tabela && tabela->size() > kMaximoDeLinhas) {
            if (erro)
                *erro = QStringLiteral("A lista é grande demais (máximo de %1 linhas por importação).").arg(kMaximoDeLinhas);
            return std::nullopt;
        }
        return tabela;
    }
    if (extensao == QLatin1String("xls")) {
        if (erro)
            *erro = QStringLiteral("O formato antigo .xls não é aceito. No Excel, use Arquivo > Salvar como e escolha "
                                   "\"Pasta de Trabalho do Excel (.xlsx)\" ou \"CSV\".");
        return std::nullopt;
    }

    QFile arquivo(caminho);
    if (!arquivo.open(QIODevice::ReadOnly)) {
        if (erro)
            *erro = QStringLiteral("Não foi possível abrir o arquivo: %1").arg(arquivo.errorString());
        return std::nullopt;
    }
    if (arquivo.size() > kTamanhoMaximoDoTexto) {
        if (erro)
            *erro = QStringLiteral("O arquivo é grande demais (máximo de 5 MB).");
        return std::nullopt;
    }
    return tabelaDeTexto(arquivo.readAll(), erro);
}

PlanoImportacaoAlunos ImportadorAlunos::planejar(int turmaId, const QList<QStringList> &tabela)
{
    PlanoImportacaoAlunos plano;
    if (tabela.isEmpty()) {
        plano.erro = QStringLiteral("A lista está vazia.");
        return plano;
    }

    // --- Colunas: pelo cabeçalho (1ª linha) ---
    int colNome = -1, colMatricula = -1, colEmail = -1, colNascimento = -1, colObservacoes = -1;
    const QStringList &cabecalho = tabela.first();
    bool reconheceuAlgum = false;
    for (int c = 0; c < cabecalho.size(); ++c) {
        int *destino = nullptr;
        switch (classificar(cabecalho.at(c))) {
        case Coluna::Nome: destino = &colNome; break;
        case Coluna::Matricula: destino = &colMatricula; break;
        case Coluna::Email: destino = &colEmail; break;
        case Coluna::Nascimento: destino = &colNascimento; break;
        case Coluna::Observacoes: destino = &colObservacoes; break;
        case Coluna::Nenhuma: break;
        }
        if (destino) {
            reconheceuAlgum = true;
            if (*destino < 0)
                *destino = c;  // se o título se repete, vale a primeira coluna
        }
    }

    int primeiraLinhaDeDados = 1;
    if (reconheceuAlgum) {
        plano.temCabecalho = true;
        if (colNome < 0) {
            plano.erro = QStringLiteral("Não encontrei a coluna de nomes. A primeira linha precisa ter o título \"Nome\" "
                                        "(ou \"Aluno\"). Use \"Baixar modelo\" para ver o formato.");
            return plano;
        }
    } else {
        // Sem títulos: só vale se for uma lista de uma coluna (um nome por linha).
        bool umaColuna = true;
        for (const QStringList &linha : tabela)
            umaColuna = umaColuna && linha.size() == 1;
        if (!umaColuna) {
            plano.erro = QStringLiteral("Não reconheci os títulos das colunas. A primeira linha precisa ter \"Nome\" "
                                        "(e, se quiser, \"Matrícula\", \"E-mail\" e \"Nascimento\"). "
                                        "Use \"Baixar modelo\" para ver o formato.");
            return plano;
        }
        colNome = 0;
        primeiraLinhaDeDados = 0;
    }

    // --- Quem já está na turma ---
    Ocupados existentes;
    for (const Aluno &a : m_alunos.listarPorTurma(turmaId, QString(), true))
        existentes.adicionar(normalizarTexto(a.nome), normalizarTexto(a.matricula));
    Ocupados aceitos;

    const int anoAtual = QDate::currentDate().year();
    for (int i = primeiraLinhaDeDados; i < tabela.size(); ++i) {
        const QStringList &linha = tabela.at(i);

        LinhaImportacaoAluno l;
        l.linhaOrigem = i + 1;
        l.aluno.nome = celula(linha, colNome).simplified();  // tira quebras, tabulações e espaços repetidos
        l.aluno.matricula = colMatricula >= 0 ? celula(linha, colMatricula).simplified() : QString();
        l.aluno.email = colEmail >= 0 ? celula(linha, colEmail).trimmed() : QString();
        l.aluno.observacoes = colObservacoes >= 0 ? celula(linha, colObservacoes).trimmed().left(kMaximoObservacoes) : QString();
        const QString nascimentoTexto = colNascimento >= 0 ? celula(linha, colNascimento).trimmed() : QString();

        QStringList avisos;
        if (!l.aluno.email.isEmpty() && !emailPlausivel(l.aluno.email)) {
            avisos << QStringLiteral("e-mail inválido ignorado");
            l.aluno.email.clear();
        }
        if (!nascimentoTexto.isEmpty()) {
            const QDate d = lerData(nascimentoTexto);
            if (d.isValid() && d.year() >= 1900 && d.year() <= anoAtual)
                l.aluno.dataNascimento = d;
            else
                avisos << QStringLiteral("data de nascimento inválida ignorada");
        }
        l.detalhe = avisos.join(QStringLiteral("; "));

        if (l.aluno.nome.isEmpty()) {
            l.situacao = LinhaImportacaoAluno::Situacao::Invalida;
            l.detalhe = QStringLiteral("sem nome");
        } else if (l.aluno.nome.size() > kMaximoNome) {
            l.situacao = LinhaImportacaoAluno::Situacao::Invalida;
            l.detalhe = QStringLiteral("nome com mais de %1 caracteres").arg(kMaximoNome);
        } else if (l.aluno.matricula.size() > kMaximoMatricula) {
            l.situacao = LinhaImportacaoAluno::Situacao::Invalida;
            l.detalhe = QStringLiteral("matrícula com mais de %1 caracteres").arg(kMaximoMatricula);
        } else {
            const QString nomeNorm = normalizarTexto(l.aluno.nome);
            const QString matriculaNorm = normalizarTexto(l.aluno.matricula);
            if (existentes.contem(nomeNorm, matriculaNorm)) {
                l.situacao = LinhaImportacaoAluno::Situacao::JaExiste;
            } else if (aceitos.contem(nomeNorm, matriculaNorm)) {
                l.situacao = LinhaImportacaoAluno::Situacao::RepetidaNoArquivo;
            } else {
                aceitos.adicionar(nomeNorm, matriculaNorm);
            }
        }
        plano.linhas.append(l);
    }

    if (plano.linhas.isEmpty())
        plano.erro = QStringLiteral("Não encontrei nenhum aluno depois da linha de títulos.");
    return plano;
}

bool ImportadorAlunos::executar(int turmaId, const PlanoImportacaoAlunos &plano, int *criados, QString *erro)
{
    if (criados)
        *criados = 0;
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) {
        if (erro)
            *erro = QStringLiteral("Não foi possível iniciar a gravação no banco de dados.");
        return false;
    }

    int n = 0;
    for (const LinhaImportacaoAluno &l : plano.linhas) {
        if (l.situacao != LinhaImportacaoAluno::Situacao::Nova)
            continue;
        Aluno a = l.aluno;
        a.id = 0;
        a.turmaId = turmaId;
        a.ativo = true;
        if (m_alunos.inserir(a) == 0) {
            db.rollback();  // tudo ou nada: nenhuma linha fica pela metade
            if (erro)
                *erro = QStringLiteral("Linha %1 (%2): %3").arg(l.linhaOrigem).arg(l.aluno.nome, m_alunos.ultimoErro());
            return false;
        }
        ++n;
    }
    if (!db.commit()) {
        db.rollback();
        if (erro)
            *erro = QStringLiteral("Não foi possível concluir a gravação no banco de dados.");
        return false;
    }
    if (criados)
        *criados = n;
    return true;
}

QByteArray ImportadorAlunos::modeloCsv()
{
    QByteArray csv("\xEF\xBB\xBF");  // BOM: o Excel reconhece o UTF-8 e mostra os acentos certos
    csv += QStringLiteral("Nome;Matrícula;E-mail;Nascimento\r\n"
                          "Ana Souza;2026001;ana@exemplo.com;15/03/2012\r\n"
                          "Bruno Lima;2026002;;02/11/2011\r\n")
               .toUtf8();
    return csv;
}
