#include "ui/HorarioGridWidget.h"

#include "core/HorarioUtil.h"
#include "ui/ThemeManager.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QDate>
#include <QFontMetrics>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>

namespace {

constexpr int kAlturaCabecalho = 34;   // linha com os nomes dos dias
constexpr int kLarguraHoras = 54;      // coluna com os rótulos de hora
constexpr int kAlturaHora = 64;        // pixels por hora
constexpr int kBordaRedimensionar = 7; // faixa inferior do bloco que redimensiona
constexpr int kLimiarArraste = 4;      // pixels até considerar que é um arraste (e não um clique)
constexpr int kPassoMinutos = 5;
constexpr int kDuracaoMinima = 15;
constexpr int kPassoNovaAula = 15;

const char *kNomesDias[] = {"Segunda", "Terça", "Quarta", "Quinta", "Sexta", "Sábado", "Domingo"};

int emMinutos(const QTime &t) { return t.hour() * 60 + t.minute(); }
QTime deMinutos(int m) { return QTime(m / 60, m % 60); }

// Cores da grade (o widget pinta tudo sozinho, então escolhe as cores pelo tema atual).
struct Cores {
    QColor fundo, linha, linhaSuave, texto, textoSuave, cabecalho, hoje, agora;
};

Cores coresDoTema()
{
    if (ThemeManager::atual() == ThemeManager::Tema::Escuro)
        return {QColor("#1B212D"), QColor("#2E3748"), QColor("#252D3C"), QColor("#E5E9F2"),
                QColor("#9AA4B8"), QColor("#232B3A"), QColor(91, 140, 255, 28), QColor("#FF5C5C")};
    return {QColor("#FFFFFF"), QColor("#DDE2EC"), QColor("#EEF1F7"), QColor("#1F2937"),
            QColor("#6B7280"), QColor("#EEF1F7"), QColor(59, 111, 224, 22), QColor("#E03A3A")};
}

}  // namespace

HorarioGridWidget::HorarioGridWidget(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);  // para trocar o cursor ao passar sobre blocos/bordas
}

void HorarioGridWidget::setDados(const QList<Horario> &aulas, int diasVisiveis, int horaInicio, int horaFim)
{
    m_aulas = aulas;
    m_dias = qBound(5, diasVisiveis, 7);
    m_horaIni = qBound(0, horaInicio, 22);
    m_horaFim = qBound(m_horaIni + 1, horaFim, 23);
    cancelarArraste();
    setMinimumHeight(sizeHint().height());
    updateGeometry();
    update();
}

QSize HorarioGridWidget::sizeHint() const
{
    return QSize(kLarguraHoras + m_dias * 130, kAlturaCabecalho + (m_horaFim - m_horaIni) * kAlturaHora + 1);
}

QSize HorarioGridWidget::minimumSizeHint() const
{
    return QSize(kLarguraHoras + m_dias * 70, sizeHint().height());
}

// ----------------------------- Geometria -----------------------------------

int HorarioGridWidget::larguraColuna() const
{
    return qMax(40, (width() - kLarguraHoras) / qMax(1, m_dias));
}

int HorarioGridWidget::yDoMinuto(int minutos) const
{
    return kAlturaCabecalho + (minutos - m_horaIni * 60) * kAlturaHora / 60;
}

int HorarioGridWidget::minutosNaPos(int y) const
{
    return m_horaIni * 60 + qRound((y - kAlturaCabecalho) * 60.0 / kAlturaHora);
}

int HorarioGridWidget::diaNaPos(int x) const
{
    const int coluna = (x - kLarguraHoras) / larguraColuna();
    return qBound(0, coluna, m_dias - 1) + 1;
}

QRect HorarioGridWidget::retangulo(int dia, int iniMin, int fimMin) const
{
    const int x = kLarguraHoras + (dia - 1) * larguraColuna() + 3;
    const int y = yDoMinuto(iniMin) + 1;
    return QRect(x, y, larguraColuna() - 6, yDoMinuto(fimMin) - yDoMinuto(iniMin) - 2);
}

const Horario *HorarioGridWidget::blocoEm(const QPoint &pos) const
{
    for (const Horario &a : m_aulas) {
        if (a.diaSemana > m_dias)
            continue;
        if (retangulo(a.diaSemana, emMinutos(a.inicio), emMinutos(a.fim)).contains(pos))
            return &a;
    }
    return nullptr;
}

bool HorarioGridWidget::bordaInferior(const Horario &aula, const QPoint &pos) const
{
    const QRect r = retangulo(aula.diaSemana, emMinutos(aula.inicio), emMinutos(aula.fim));
    return pos.y() >= r.bottom() - kBordaRedimensionar;
}

bool HorarioGridWidget::haSobreposicao(int dia, int iniMin, int fimMin, int ignorarId) const
{
    for (const Horario &a : m_aulas) {
        if (a.id == ignorarId || a.diaSemana != dia)
            continue;
        if (HorarioUtil::sobrepoe(iniMin, fimMin, emMinutos(a.inicio), emMinutos(a.fim)))
            return true;
    }
    return false;
}

void HorarioGridWidget::cancelarArraste()
{
    m_modo = Modo::Nada;
    m_arrastando = false;
    unsetCursor();
}

// ------------------------------- Desenho ------------------------------------

void HorarioGridWidget::paintEvent(QPaintEvent *)
{
    const Cores c = coresDoTema();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), c.fundo);

    const int colW = larguraColuna();
    const int gradeDireita = kLarguraHoras + colW * m_dias;
    const int gradeBaixo = kAlturaCabecalho + (m_horaFim - m_horaIni) * kAlturaHora;
    const int hoje = QDate::currentDate().dayOfWeek();

    // Cabeçalho (dias da semana)
    p.fillRect(QRect(0, 0, width(), kAlturaCabecalho), c.cabecalho);
    QFont negrito = font();
    negrito.setBold(true);
    p.setFont(negrito);
    for (int d = 1; d <= m_dias; ++d) {
        const QRect r(kLarguraHoras + (d - 1) * colW, 0, colW, kAlturaCabecalho);
        p.setPen(d == hoje ? palette().color(QPalette::Highlight) : c.texto);
        const QString nome = QFontMetrics(negrito).elidedText(QString::fromUtf8(kNomesDias[d - 1]),
                                                              Qt::ElideRight, colW - 8);
        p.drawText(r, Qt::AlignCenter, nome);
    }

    // Coluna de hoje levemente destacada
    if (hoje <= m_dias)
        p.fillRect(QRect(kLarguraHoras + (hoje - 1) * colW, kAlturaCabecalho, colW, gradeBaixo - kAlturaCabecalho),
                   c.hoje);

    // Linhas de hora (cheias) e meia hora (suaves), com os rótulos
    p.setFont(font());
    for (int h = m_horaIni; h <= m_horaFim; ++h) {
        const int y = kAlturaCabecalho + (h - m_horaIni) * kAlturaHora;
        p.setPen(c.linha);
        p.drawLine(kLarguraHoras, y, gradeDireita, y);
        p.setPen(c.textoSuave);
        p.drawText(QRect(0, y - 9, kLarguraHoras - 8, 18), Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("%1:00").arg(h, 2, 10, QLatin1Char('0')));
        if (h < m_horaFim) {
            p.setPen(QPen(c.linhaSuave, 1, Qt::DashLine));
            p.drawLine(kLarguraHoras, y + kAlturaHora / 2, gradeDireita, y + kAlturaHora / 2);
        }
    }
    // Linhas verticais entre os dias
    p.setPen(c.linha);
    for (int d = 0; d <= m_dias; ++d) {
        const int x = kLarguraHoras + d * colW;
        p.drawLine(x, kAlturaCabecalho, x, gradeBaixo);
    }

    // Blocos de aula
    for (const Horario &a : m_aulas) {
        if (a.diaSemana > m_dias)
            continue;
        const QRect r = retangulo(a.diaSemana, emMinutos(a.inicio), emMinutos(a.fim));
        const bool sendoArrastado = m_arrastando && a.id == m_origem.id;

        QColor cor(a.turmaCor);
        if (!cor.isValid())
            cor = QColor("#4C8BF5");
        if (sendoArrastado)
            cor.setAlpha(70);  // o original fica "apagado" enquanto o fantasma se move

        p.setPen(Qt::NoPen);
        p.setBrush(cor);
        p.drawRoundedRect(r, 6, 6);

        const QColor textoCor = (cor.lightness() > 170 && !sendoArrastado) ? QColor("#1F2937") : QColor("#FFFFFF");
        p.save();
        p.setClipRect(r.adjusted(5, 3, -5, -3));
        p.setPen(textoCor);
        p.setFont(negrito);
        const QFontMetrics fmNegrito(negrito);
        const QFontMetrics fm(font());
        int y = r.top() + 4 + fmNegrito.ascent();
        p.drawText(r.left() + 7, y, a.turmaNome);
        p.setFont(font());
        y += fm.height();
        if (y < r.bottom())
            p.drawText(r.left() + 7, y,
                       QStringLiteral("%1–%2").arg(a.inicio.toString(QStringLiteral("HH:mm")),
                                                   a.fim.toString(QStringLiteral("HH:mm"))));
        y += fm.height();
        if (y < r.bottom() && !a.sala.isEmpty())
            p.drawText(r.left() + 7, y, QStringLiteral("Sala %1").arg(a.sala));
        p.restore();

        // "Alça" de redimensionar (três pontinhos na base do bloco)
        if (!sendoArrastado && r.height() > 26) {
            p.setPen(QPen(textoCor, 2));
            p.drawLine(r.center().x() - 8, r.bottom() - 4, r.center().x() + 8, r.bottom() - 4);
        }
    }

    // Fantasma do arraste (vermelho quando bate em outra aula)
    if (m_arrastando && m_fantasmaDia > 0) {
        const QRect r = retangulo(m_fantasmaDia, m_fantasmaIni, m_fantasmaFim);
        QColor cor = m_fantasmaConflito ? QColor("#E03A3A") : QColor(m_origem.turmaCor);
        cor.setAlpha(190);
        p.setPen(QPen(Qt::white, 1));
        p.setBrush(cor);
        p.drawRoundedRect(r, 6, 6);
        p.setPen(Qt::white);
        p.setFont(negrito);
        p.drawText(r.adjusted(7, 4, -5, -3), Qt::AlignLeft | Qt::AlignTop,
                   QStringLiteral("%1\n%2–%3").arg(m_origem.turmaNome,
                                                   deMinutos(m_fantasmaIni).toString(QStringLiteral("HH:mm")),
                                                   deMinutos(m_fantasmaFim).toString(QStringLiteral("HH:mm"))));
    }

    // Linha do horário atual (só na coluna de hoje)
    const QTime agora = QTime::currentTime();
    const int agoraMin = emMinutos(agora);
    if (hoje <= m_dias && agoraMin >= m_horaIni * 60 && agoraMin <= m_horaFim * 60) {
        const int y = yDoMinuto(agoraMin);
        const int x0 = kLarguraHoras + (hoje - 1) * colW;
        p.setPen(QPen(c.agora, 2));
        p.drawLine(x0, y, x0 + colW, y);
        p.setBrush(c.agora);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPoint(x0, y), 4, 4);
    }
}

void HorarioGridWidget::changeEvent(QEvent *evento)
{
    // Troca de tema (folha de estilo) ou paleta: repinta com as cores novas.
    if (evento->type() == QEvent::StyleChange || evento->type() == QEvent::PaletteChange)
        update();
    QWidget::changeEvent(evento);
}

// ------------------------------- Mouse --------------------------------------

void HorarioGridWidget::mousePressEvent(QMouseEvent *evento)
{
    if (evento->button() != Qt::LeftButton)
        return;

    const Horario *bloco = blocoEm(evento->pos());
    if (!bloco)
        return;

    m_origem = *bloco;
    m_modo = bordaInferior(*bloco, evento->pos()) ? Modo::Redimensionar : Modo::Mover;
    m_pontoPressionado = evento->pos();
    m_arrastando = false;
    m_deslocamentoMin = minutosNaPos(evento->pos().y()) - emMinutos(bloco->inicio);
    m_fantasmaDia = bloco->diaSemana;
    m_fantasmaIni = emMinutos(bloco->inicio);
    m_fantasmaFim = emMinutos(bloco->fim);
    m_fantasmaConflito = false;
}

void HorarioGridWidget::mouseMoveEvent(QMouseEvent *evento)
{
    // Sem botão pressionado: só ajusta o cursor conforme o que está embaixo.
    if (m_modo == Modo::Nada) {
        const Horario *bloco = blocoEm(evento->pos());
        if (!bloco)
            setCursor(Qt::ArrowCursor);
        else
            setCursor(bordaInferior(*bloco, evento->pos()) ? Qt::SizeVerCursor : Qt::OpenHandCursor);
        return;
    }

    if (!m_arrastando) {
        if ((evento->pos() - m_pontoPressionado).manhattanLength() < kLimiarArraste)
            return;
        m_arrastando = true;
    }

    const int iniOriginal = emMinutos(m_origem.inicio);
    const int duracao = emMinutos(m_origem.fim) - iniOriginal;
    const int minimo = m_horaIni * 60;
    const int maximo = m_horaFim * 60;

    if (m_modo == Modo::Mover) {
        setCursor(Qt::ClosedHandCursor);
        m_fantasmaDia = diaNaPos(evento->pos().x());
        m_fantasmaIni = HorarioUtil::ajustarInicio(minutosNaPos(evento->pos().y()) - m_deslocamentoMin, duracao,
                                                   minimo, maximo, kPassoMinutos);
        m_fantasmaFim = m_fantasmaIni + duracao;
    } else {
        setCursor(Qt::SizeVerCursor);
        m_fantasmaDia = m_origem.diaSemana;
        m_fantasmaIni = iniOriginal;
        const int fimDesejado = HorarioUtil::arredondar(minutosNaPos(evento->pos().y()), kPassoMinutos);
        m_fantasmaFim = qBound(iniOriginal + kDuracaoMinima, fimDesejado, maximo);
    }

    m_fantasmaConflito = haSobreposicao(m_fantasmaDia, m_fantasmaIni, m_fantasmaFim, m_origem.id);
    update();
}

void HorarioGridWidget::mouseReleaseEvent(QMouseEvent *evento)
{
    if (evento->button() != Qt::LeftButton || m_modo == Modo::Nada)
        return;

    const bool foiArraste = m_arrastando;
    const Horario origem = m_origem;
    const int dia = m_fantasmaDia, ini = m_fantasmaIni, fim = m_fantasmaFim;
    const bool conflito = m_fantasmaConflito;
    cancelarArraste();
    update();

    if (!foiArraste)
        return;  // foi só um clique

    const bool mudou = dia != origem.diaSemana || ini != emMinutos(origem.inicio) || fim != emMinutos(origem.fim);
    if (!mudou)
        return;
    if (conflito) {
        emit movimentoRecusado();
        return;
    }
    emit aulaAlterada(origem.id, dia, deMinutos(ini), deMinutos(fim));
}

void HorarioGridWidget::mouseDoubleClickEvent(QMouseEvent *evento)
{
    if (evento->button() != Qt::LeftButton)
        return;
    cancelarArraste();

    if (const Horario *bloco = blocoEm(evento->pos())) {
        emit editarSolicitado(bloco->id);
        return;
    }
    if (evento->pos().y() > kAlturaCabecalho && evento->pos().x() > kLarguraHoras) {
        const int min = HorarioUtil::arredondar(minutosNaPos(evento->pos().y()), kPassoNovaAula);
        emit novaAulaSolicitada(diaNaPos(evento->pos().x()), deMinutos(qBound(0, min, 23 * 60 + 55)));
    }
}

void HorarioGridWidget::contextMenuEvent(QContextMenuEvent *evento)
{
    QMenu menu(this);
    if (const Horario *bloco = blocoEm(evento->pos())) {
        const int id = bloco->id;
        menu.addAction(QStringLiteral("Editar aula…"), this, [this, id] { emit editarSolicitado(id); });
        menu.addAction(QStringLiteral("Excluir aula"), this, [this, id] { emit excluirSolicitado(id); });
    } else if (evento->pos().y() > kAlturaCabecalho && evento->pos().x() > kLarguraHoras) {
        const int dia = diaNaPos(evento->pos().x());
        const int min = HorarioUtil::arredondar(minutosNaPos(evento->pos().y()), kPassoNovaAula);
        const QTime inicio = deMinutos(qBound(0, min, 23 * 60 + 55));
        menu.addAction(QStringLiteral("Nova aula aqui…"), this,
                       [this, dia, inicio] { emit novaAulaSolicitada(dia, inicio); });
    } else {
        return;
    }
    menu.exec(evento->globalPos());
}
