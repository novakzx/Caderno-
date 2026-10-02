#include "services/XlsxService.h"

#include "core/TextoUtil.h"
#include "core/Tokens.h"
#include "database/NotaRepository.h"

#include <QColor>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QVariant>

#include <cmath>

#include <xlsxdocument.h>
#include <xlsxformat.h>
#include <xlsxrichstring.h>

namespace {

// Texto vindo do usuário (nomes, matrículas) entra na planilha SEMPRE como texto.
// O Document::write() do QXlsx converte qualquer texto que comece com "=" em FÓRMULA (e
// URLs em links): um aluno chamado =HYPERLINK(...) viraria uma fórmula ativa no Excel.
void escreverTexto(QXlsx::Document &doc, int linha, int coluna, const QString &texto,
                   const QXlsx::Format &formato = QXlsx::Format())
{
    doc.write(linha, coluna, QVariant::fromValue(QXlsx::RichString(texto)), formato);
}

}  // namespace

namespace XlsxService {

namespace {

// Número da coluna (1 = A) -> letra(s) do Excel: 1 -> "A", 27 -> "AA".
QString letra(int coluna)
{
    QString s;
    while (coluna > 0) {
        const int resto = (coluna - 1) % 26;
        s.prepend(QChar(u'A' + resto));
        coluna = (coluna - 1) / 26;
    }
    return s;
}

// O Excel limita o nome da aba a 31 caracteres e proíbe  [ ] : * ? / \ .
QString nomeDeAba(QString nome)
{
    nome.remove(QRegularExpression(QStringLiteral("[\\[\\]:*?/\\\\]")));
    nome = nome.trimmed().left(31);
    return nome.isEmpty() ? QStringLiteral("Notas") : nome;
}

// Converte o valor de uma célula em número. Aceita número de verdade ou texto
// com vírgula decimal ("8,5"). Célula vazia -> nullopt sem erro; texto que não
// é número -> nullopt com *invalido = true.
std::optional<double> lerNumero(const QVariant &v, bool *invalido)
{
    *invalido = false;
    if (!v.isValid() || v.isNull())
        return std::nullopt;

    if (v.typeId() == QMetaType::QString) {
        QString t = v.toString().trimmed();
        if (t.isEmpty())
            return std::nullopt;
        t.replace(QLatin1Char(','), QLatin1Char('.'));
        bool ok = false;
        const double d = t.toDouble(&ok);
        if (!ok) {
            *invalido = true;
            return std::nullopt;
        }
        return d;
    }

    bool ok = false;
    const double d = v.toDouble(&ok);
    if (!ok) {
        *invalido = true;
        return std::nullopt;
    }
    return d;
}

}  // namespace

// ============================================================================
// Exportação
// ============================================================================

bool exportar(const QString &caminho, const DadosExportacao &dados, QString *erro)
{
    QXlsx::Document doc;
    doc.renameSheet(QStringLiteral("Sheet1"), nomeDeAba(dados.titulo));

    QXlsx::Format fmtCabecalho;
    fmtCabecalho.setFontBold(true);
    fmtCabecalho.setFillPattern(QXlsx::Format::PatternSolid);
    // Planilha é um arquivo (sem tema): usa o primary-soft do tema claro.
    fmtCabecalho.setPatternBackgroundColor(QColor(QLatin1String(Tokens::hex(Tokens::Id::PrimarySoft, false))));
    fmtCabecalho.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    QXlsx::Format fmtRotulo;
    fmtRotulo.setFontItalic(true);

    QXlsx::Format fmtMedia;
    fmtMedia.setFontBold(true);
    fmtMedia.setNumberFormat(QStringLiteral("0.00"));

    const int n = dados.avaliacoes.size();
    const int colPrimeira = 3;           // C
    const int colUltima = 2 + n;
    const int colMedia = 3 + n;
    const int linhaPeso = 2, linhaMax = 3, linhaPeriodo = 4, primeiraLinhaAluno = 5;

    // Cabeçalho e linhas de metadados
    doc.write(1, 1, QStringLiteral("Matrícula"), fmtCabecalho);
    doc.write(1, 2, QStringLiteral("Aluno"), fmtCabecalho);
    doc.write(linhaPeso, 2, QStringLiteral("Peso"), fmtRotulo);
    doc.write(linhaMax, 2, QStringLiteral("Nota máxima"), fmtRotulo);
    doc.write(linhaPeriodo, 2, QStringLiteral("Período"), fmtRotulo);
    for (int i = 0; i < n; ++i) {
        const Avaliacao &a = dados.avaliacoes.at(i);
        const int col = colPrimeira + i;
        escreverTexto(doc, 1, col, a.nome, fmtCabecalho);
        doc.write(linhaPeso, col, a.peso);
        doc.write(linhaMax, col, a.notaMaxima);
        doc.write(linhaPeriodo, col, a.periodo);
    }
    doc.write(1, colMedia, QStringLiteral("Média"), fmtCabecalho);

    // Uma linha por aluno
    for (int i = 0; i < dados.alunos.size(); ++i) {
        const Aluno &aluno = dados.alunos.at(i);
        const int linha = primeiraLinhaAluno + i;
        escreverTexto(doc, linha, 1, aluno.matricula);
        escreverTexto(doc, linha, 2, aluno.nome);

        for (int j = 0; j < n; ++j) {
            const auto it = dados.notas.constFind(
                NotaRepository::chave(dados.avaliacoes.at(j).id, aluno.id));
            if (it != dados.notas.constEnd())
                doc.write(linha, colPrimeira + j, it.value());
        }

        // Média ponderada como FÓRMULA do Excel: se o professor editar uma nota
        // na planilha, a média se recalcula sozinha. Considera só as células
        // preenchidas e normaliza cada nota para a escala 0-10 (igual ao programa).
        if (n > 0) {
            const QString a = letra(colPrimeira), z = letra(colUltima);
            const QString notas = QStringLiteral("%1%2:%3%2").arg(a, QString::number(linha), z);
            const QString maximos = QStringLiteral("%1$%2:%3$%2").arg(a, QString::number(linhaMax), z);
            const QString pesos = QStringLiteral("%1$%2:%3$%2").arg(a, QString::number(linhaPeso), z);
            const QString formula =
                QStringLiteral("=IFERROR(SUMPRODUCT((%1<>\"\")*%1/%2*10*%3)/SUMPRODUCT((%1<>\"\")*%3),\"\")")
                    .arg(notas, maximos, pesos);
            doc.write(linha, colMedia, formula, fmtMedia);
        }
    }

    doc.setColumnWidth(1, 1, 14);
    doc.setColumnWidth(2, 2, 32);
    doc.setColumnWidth(colPrimeira, colMedia, 14);

    if (!doc.saveAs(caminho)) {
        if (erro)
            *erro = QStringLiteral("Não foi possível gravar o arquivo. Ele pode estar aberto em outro programa "
                                   "ou a pasta não permite gravação.");
        return false;
    }
    return true;
}

// ============================================================================
// Importação
// ============================================================================

std::optional<Planilha> importar(const QString &caminho, QString *erro)
{
    auto falha = [erro](const QString &msg) -> std::optional<Planilha> {
        if (erro)
            *erro = msg;
        return std::nullopt;
    };

    // Limites contra arquivos feitos para travar o programa (planilha gigante ou "bomba" de compressão).
    constexpr qint64 kTamanhoMaximo = 25LL * 1024 * 1024;
    if (QFileInfo(caminho).size() > kTamanhoMaximo)
        return falha(QStringLiteral("O arquivo é grande demais (máximo de 25 MB)."));

    QXlsx::Document doc(caminho);
    if (!doc.load())
        return falha(QStringLiteral("Não foi possível abrir o arquivo. Verifique se é um .xlsx válido "
                                    "e se não está protegido por senha."));

    const QXlsx::CellRange dim = doc.dimension();
    if (!dim.isValid())
        return falha(QStringLiteral("A planilha está vazia."));
    const int ultimaLinha = dim.lastRow();
    const int ultimaColuna = dim.lastColumn();
    if (ultimaLinha > 5000 || ultimaColuna > 200)
        return falha(QStringLiteral("A planilha é grande demais (máximo de 5.000 linhas e 200 colunas)."));

    auto texto = [&doc](int linha, int coluna) {
        return doc.read(linha, coluna).toString().trimmed();
    };

    // --- 1) Classifica as colunas pelo cabeçalho (linha 1) ---
    static const QSet<QString> nomesMatricula = {"matricula", "mat", "mat.", "matr", "ra", "codigo"};
    static const QSet<QString> nomesAluno = {"aluno", "aluna", "nome", "nome do aluno", "estudante"};
    static const QSet<QString> nomesIgnorados = {"media", "media final", "media parcial", "resultado",
                                                 "situacao", "frequencia", "faltas"};

    int colMatricula = 0, colNome = 0;
    QList<int> colunasDeNota;  // índices (1-based) das colunas da planilha que são avaliações
    Planilha planilha;

    for (int c = 1; c <= ultimaColuna; ++c) {
        const QString cabecalho = texto(1, c);
        if (cabecalho.isEmpty())
            continue;
        const QString norm = normalizarTexto(cabecalho);
        if (nomesMatricula.contains(norm) && colMatricula == 0)
            colMatricula = c;
        else if (nomesAluno.contains(norm) && colNome == 0)
            colNome = c;
        else if (nomesIgnorados.contains(norm))
            continue;
        else {
            colunasDeNota.append(c);
            ColunaPlanilha col;
            col.nome = cabecalho;
            planilha.colunas.append(col);
        }
    }

    if (colMatricula == 0 && colNome == 0)
        return falha(QStringLiteral("Não encontrei as colunas \"Aluno\" ou \"Matrícula\" na primeira linha "
                                    "da planilha. Exporte uma planilha pelo programa para ver o formato esperado."));

    // --- 2) Linhas opcionais de metadados (Peso / Nota máxima / Período) ---
    const int colRotulo = colNome ? colNome : colMatricula;
    int primeiraLinhaDeDados = 2;
    while (primeiraLinhaDeDados <= ultimaLinha) {
        const QString rotulo = normalizarTexto(texto(primeiraLinhaDeDados, colRotulo));
        const bool ehPeso = rotulo == QLatin1String("peso");
        const bool ehMax = rotulo == QLatin1String("nota maxima") || rotulo == QLatin1String("maximo");
        const bool ehPeriodo = rotulo == QLatin1String("periodo");
        if (!ehPeso && !ehMax && !ehPeriodo)
            break;

        for (int i = 0; i < colunasDeNota.size(); ++i) {
            bool invalido = false;
            const auto valor = lerNumero(doc.read(primeiraLinhaDeDados, colunasDeNota.at(i)), &invalido);
            if (!valor)
                continue;
            ColunaPlanilha &col = planilha.colunas[i];
            if (ehPeso && *valor > 0) {
                col.peso = *valor;
                col.temMetadados = true;
            } else if (ehMax && *valor > 0) {
                col.notaMaxima = *valor;
                col.temMetadados = true;
            } else if (ehPeriodo) {
                col.periodo = qBound(1, static_cast<int>(*valor), 4);
                col.temMetadados = true;
            }
        }
        ++primeiraLinhaDeDados;
    }

    // --- 3) Linhas de alunos ---
    for (int linha = primeiraLinhaDeDados; linha <= ultimaLinha; ++linha) {
        LinhaPlanilha l;
        l.linhaExcel = linha;
        l.matricula = colMatricula ? texto(linha, colMatricula) : QString();
        l.nome = colNome ? texto(linha, colNome) : QString();
        if (l.matricula.isEmpty() && l.nome.isEmpty())
            continue;  // linha em branco

        for (int i = 0; i < colunasDeNota.size(); ++i) {
            bool invalido = false;
            const auto valor = lerNumero(doc.read(linha, colunasDeNota.at(i)), &invalido);
            if (invalido)
                planilha.avisos.append(QStringLiteral("Linha %1, coluna \"%2\": valor \"%3\" ignorado (não é número).")
                                           .arg(linha)
                                           .arg(planilha.colunas.at(i).nome,
                                                texto(linha, colunasDeNota.at(i))));
            l.notas.append(valor);
        }
        planilha.linhas.append(l);
    }

    if (planilha.linhas.isEmpty())
        return falha(QStringLiteral("Não encontrei nenhuma linha de aluno na planilha."));
    if (planilha.colunas.isEmpty())
        return falha(QStringLiteral("Não encontrei nenhuma coluna de nota na planilha."));

    return planilha;
}

std::optional<QList<QStringList>> lerTabela(const QString &caminho, QString *erro)
{
    auto falha = [erro](const QString &msg) -> std::optional<QList<QStringList>> {
        if (erro)
            *erro = msg;
        return std::nullopt;
    };

    constexpr qint64 kTamanhoMaximo = 25LL * 1024 * 1024;
    if (QFileInfo(caminho).size() > kTamanhoMaximo)
        return falha(QStringLiteral("O arquivo é grande demais (máximo de 25 MB)."));

    QXlsx::Document doc(caminho);
    if (!doc.load())
        return falha(QStringLiteral("Não foi possível abrir o arquivo. Verifique se é um .xlsx válido "
                                    "e se não está protegido por senha."));

    const QXlsx::CellRange dim = doc.dimension();
    if (!dim.isValid())
        return falha(QStringLiteral("A planilha está vazia."));
    if (dim.lastRow() > 5000 || dim.lastColumn() > 200)
        return falha(QStringLiteral("A planilha é grande demais (máximo de 5.000 linhas e 200 colunas)."));

    QList<QStringList> tabela;
    for (int linha = 1; linha <= dim.lastRow(); ++linha) {
        QStringList celulas;
        bool temConteudo = false;
        for (int coluna = 1; coluna <= dim.lastColumn(); ++coluna) {
            const QVariant v = doc.read(linha, coluna);
            QString texto;
            switch (v.typeId()) {
            case QMetaType::QDateTime: texto = v.toDateTime().date().toString(Qt::ISODate); break;
            case QMetaType::QDate: texto = v.toDate().toString(Qt::ISODate); break;
            case QMetaType::Double:
            case QMetaType::Float: {
                const double d = v.toDouble();
                texto = (d == std::floor(d) && std::fabs(d) < 1e15) ? QString::number(d, 'f', 0) : QString::number(d, 'g', 15);
                break;
            }
            default: texto = v.toString().trimmed(); break;
            }
            temConteudo = temConteudo || !texto.isEmpty();
            celulas << texto;
        }
        if (temConteudo)
            tabela.append(celulas);
    }
    if (tabela.isEmpty())
        return falha(QStringLiteral("A planilha está vazia."));
    return tabela;
}

}  // namespace XlsxService
