#include "services/RelatorioPdf.h"

#include "core/FrequenciaUtil.h"

#include <QDate>
#include <QFile>
#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QTextDocument>

namespace RelatorioPdf {

namespace {

const char *kEstilo =
    "<style>"
    "body { font-family: 'Segoe UI', 'Helvetica', sans-serif; font-size: 10pt; color: #1F2937; }"
    "h1 { font-size: 18pt; margin-bottom: 0; }"
    "h2 { font-size: 12pt; margin-top: 16px; }"
    "p.sub { color: #6B7280; margin-top: 2px; }"
    "th { background-color: #DDE6FA; }"
    "</style>";

QString esc(const QString &s) { return s.toHtmlEscaped(); }

QString numero(double v, int casas = 2)
{
    return QString::number(v, 'f', casas).replace(QLatin1Char('.'), QLatin1Char(','));
}

QString numeroCurto(double v)
{
    QString s = QString::number(v, 'f', 2);
    while (s.endsWith(QLatin1Char('0')))
        s.chop(1);
    if (s.endsWith(QLatin1Char('.')))
        s.chop(1);
    return s.replace(QLatin1Char('.'), QLatin1Char(','));
}

QString celula(const QString &conteudo, const QString &atributos = QString())
{
    return QStringLiteral("<td %1>%2</td>").arg(atributos, conteudo);
}

QString cabecalho(const QString &titulo, const QString &subtitulo)
{
    return QStringLiteral("<h1>%1</h1><p class='sub'>%2 · gerado em %3</p>")
        .arg(esc(titulo), esc(subtitulo), QDate::currentDate().toString(QStringLiteral("dd/MM/yyyy")));
}

QString mediaHtml(const std::optional<double> &m, double corte)
{
    if (!m)
        return QStringLiteral("—");
    const QString texto = numero(*m);
    return *m < corte ? QStringLiteral("<font color='#C0392B'><b>%1</b></font>").arg(texto)
                      : QStringLiteral("<b>%1</b>").arg(texto);
}

QString frequenciaHtml(const std::optional<double> &pct)
{
    if (!pct)
        return QStringLiteral("—");
    const QString texto = numero(*pct, 1) + QLatin1Char('%');
    return *pct < FrequenciaUtil::kFrequenciaMinima ? QStringLiteral("<font color='#C0392B'><b>%1</b></font>").arg(texto)
                                                    : texto;
}

QString nomeDoPeriodo(int periodo)
{
    return periodo == 0 ? QStringLiteral("todos os períodos") : QStringLiteral("%1º período").arg(periodo);
}

QString situacaoPorExtenso(QChar s)
{
    switch (s.toLatin1()) {
    case 'F': return QStringLiteral("Falta");
    case 'J': return QStringLiteral("Falta justificada");
    case 'A': return QStringLiteral("Atraso");
    default:  return QStringLiteral("Presente");
    }
}

}  // namespace

QString htmlBoletim(const Boletim &b, double notaCorte)
{
    QString h = QStringLiteral("<html><head>%1</head><body>").arg(QLatin1String(kEstilo));
    h += cabecalho(QStringLiteral("Boletim — %1").arg(b.turma.nome),
                   QStringLiteral("%1 · %2 · %3")
                       .arg(b.turma.disciplina.isEmpty() ? QStringLiteral("sem disciplina") : b.turma.disciplina,
                            QString::number(b.turma.anoLetivo), nomeDoPeriodo(b.periodo)));

    h += QStringLiteral("<table border='1' cellspacing='0' cellpadding='4' width='100%'><tr>"
                        "<th align='left'>Aluno</th>");
    for (const Avaliacao &a : b.avaliacoes)
        h += QStringLiteral("<th>%1<br><small>peso %2 · máx %3</small></th>")
                 .arg(esc(a.nome), numeroCurto(a.peso), numeroCurto(a.notaMaxima));
    h += QStringLiteral("<th>Média</th><th>Freq.</th></tr>");

    for (const LinhaBoletim &l : b.linhas) {
        h += QStringLiteral("<tr>") + celula(esc(l.nome));
        for (const auto &nota : l.notas)
            h += celula(nota ? numeroCurto(*nota) : QStringLiteral("—"), QStringLiteral("align='center'"));
        h += celula(mediaHtml(l.media, notaCorte), QStringLiteral("align='center'"));
        h += celula(frequenciaHtml(l.frequenciaPct), QStringLiteral("align='center'"));
        h += QStringLiteral("</tr>");
    }
    // Linha de médias da turma
    h += QStringLiteral("<tr bgcolor='#F4F6FA'>") + celula(QStringLiteral("<b>Média da turma</b>"));
    for (const auto &m : b.mediaPorAvaliacao)
        h += celula(m ? numero(*m, 1) : QStringLiteral("—"), QStringLiteral("align='center'"));
    h += celula(b.mediaTurma ? QStringLiteral("<b>%1</b>").arg(numero(*b.mediaTurma)) : QStringLiteral("—"),
                QStringLiteral("align='center'"));
    h += celula(QString()) + QStringLiteral("</tr></table>");

    h += QStringLiteral("<p class='sub'>Média ponderada das notas lançadas, na escala de 0 a 10 (médias abaixo de %1 em vermelho). "
                        "Médias por avaliação também estão na escala 0-10. Frequência abaixo de %2% em vermelho.</p>")
             .arg(numero(notaCorte, 1))
             .arg(FrequenciaUtil::kFrequenciaMinima, 0, 'f', 0);
    h += QStringLiteral("</body></html>");
    return h;
}

QString htmlFrequencia(const Boletim &b)
{
    QString h = QStringLiteral("<html><head>%1</head><body>").arg(QLatin1String(kEstilo));
    h += cabecalho(QStringLiteral("Frequência — %1").arg(b.turma.nome),
                   QStringLiteral("%1 · %2")
                       .arg(b.turma.disciplina.isEmpty() ? QStringLiteral("sem disciplina") : b.turma.disciplina,
                            QString::number(b.turma.anoLetivo)));

    h += QStringLiteral("<table border='1' cellspacing='0' cellpadding='4' width='100%'><tr>"
                        "<th align='left'>Aluno</th><th>Presenças</th><th>Atrasos</th><th>Faltas</th>"
                        "<th>Justificadas</th><th>Chamadas</th><th>Frequência</th></tr>");
    int abaixo = 0;
    for (const LinhaBoletim &l : b.linhas) {
        const ResumoFrequencia &f = l.frequencia;
        if (l.frequenciaPct && *l.frequenciaPct < FrequenciaUtil::kFrequenciaMinima)
            ++abaixo;
        h += QStringLiteral("<tr>") + celula(esc(l.nome));
        for (const int v : {f.presencas, f.atrasos, f.faltas, f.justificadas, f.total()})
            h += celula(QString::number(v), QStringLiteral("align='center'"));
        h += celula(frequenciaHtml(l.frequenciaPct), QStringLiteral("align='center'"));
        h += QStringLiteral("</tr>");
    }
    h += QStringLiteral("</table>");
    h += QStringLiteral("<p class='sub'>Atraso conta como presença; falta justificada não reduz a frequência. "
                        "%1 aluno(s) abaixo do mínimo de %2%.</p>")
             .arg(abaixo)
             .arg(FrequenciaUtil::kFrequenciaMinima, 0, 'f', 0);
    h += QStringLiteral("</body></html>");
    return h;
}

QString htmlFicha(const FichaAluno &f, double notaCorte)
{
    QString h = QStringLiteral("<html><head>%1</head><body>").arg(QLatin1String(kEstilo));
    h += cabecalho(QStringLiteral("Ficha do aluno — %1").arg(f.aluno.nome),
                   QStringLiteral("%1 · %2%3")
                       .arg(f.turma.nome, QString::number(f.turma.anoLetivo),
                            f.aluno.matricula.isEmpty() ? QString() : QStringLiteral(" · matrícula %1").arg(f.aluno.matricula)));

    h += QStringLiteral("<h2>Notas</h2>");
    if (f.avaliacoes.isEmpty()) {
        h += QStringLiteral("<p>Nenhuma avaliação cadastrada.</p>");
    } else {
        h += QStringLiteral("<table border='1' cellspacing='0' cellpadding='4' width='100%'><tr>"
                            "<th align='left'>Avaliação</th><th>Período</th><th>Peso</th><th>Nota</th><th>Máx.</th></tr>");
        for (int i = 0; i < f.avaliacoes.size(); ++i) {
            const Avaliacao &a = f.avaliacoes.at(i);
            h += QStringLiteral("<tr>") + celula(esc(a.nome));
            h += celula(QString::number(a.periodo), QStringLiteral("align='center'"));
            h += celula(numeroCurto(a.peso), QStringLiteral("align='center'"));
            h += celula(f.notas.at(i) ? numeroCurto(*f.notas.at(i)) : QStringLiteral("—"), QStringLiteral("align='center'"));
            h += celula(numeroCurto(a.notaMaxima), QStringLiteral("align='center'"));
            h += QStringLiteral("</tr>");
        }
        h += QStringLiteral("</table>");
    }
    h += QStringLiteral("<p><b>Média ponderada:</b> %1 <small>(nota de corte %2)</small></p>")
             .arg(mediaHtml(f.media, notaCorte), numero(notaCorte, 1));

    h += QStringLiteral("<h2>Frequência</h2>");
    h += QStringLiteral("<p>%1 presença(s), %2 atraso(s), %3 falta(s), %4 justificada(s) em %5 chamada(s) — frequência: %6</p>")
             .arg(f.frequencia.presencas)
             .arg(f.frequencia.atrasos)
             .arg(f.frequencia.faltas)
             .arg(f.frequencia.justificadas)
             .arg(f.frequencia.total())
             .arg(frequenciaHtml(f.frequenciaPct));

    if (!f.ocorrencias.isEmpty()) {
        h += QStringLiteral("<table border='1' cellspacing='0' cellpadding='4' width='100%'><tr>"
                            "<th>Data</th><th>Situação</th><th align='left'>Justificativa</th></tr>");
        for (const RegistroFrequencia &r : f.ocorrencias)
            h += QStringLiteral("<tr>") + celula(r.data.toString(QStringLiteral("dd/MM/yyyy")), QStringLiteral("align='center'")) +
                 celula(situacaoPorExtenso(r.situacao), QStringLiteral("align='center'")) +
                 celula(esc(r.justificativa)) + QStringLiteral("</tr>");
        h += QStringLiteral("</table>");
    }

    if (!f.aluno.observacoes.isEmpty())
        h += QStringLiteral("<h2>Observações</h2><p>%1</p>").arg(esc(f.aluno.observacoes).replace(QLatin1Char('\n'), QStringLiteral("<br>")));

    h += QStringLiteral("</body></html>");
    return h;
}

bool salvarPdf(const QString &html, const QString &caminho, const QString &titulo, bool paisagem, QString *erro)
{
    QPdfWriter writer(caminho);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageOrientation(paisagem ? QPageLayout::Landscape : QPageLayout::Portrait);
    writer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
    writer.setTitle(titulo);
    writer.setCreator(QStringLiteral("Professor Organizado"));

    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&writer);  // pagina automaticamente

    // O QPdfWriter grava o arquivo ao ser destruído/encerrado; confere se existe.
    if (!QFile::exists(caminho)) {
        if (erro)
            *erro = QStringLiteral("Não foi possível criar o arquivo PDF. Verifique se a pasta permite gravação "
                                   "e se o arquivo não está aberto em outro programa.");
        return false;
    }
    return true;
}

}  // namespace RelatorioPdf
