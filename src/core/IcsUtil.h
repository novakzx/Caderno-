#pragma once

#include <cstdio>
#include <string>
#include <vector>

// Calendário em formato iCalendar (.ics, RFC 5545), sem Qt (testável: tests/test_utilitarios.cpp).
// Abre no Google Agenda, Outlook e no calendário do celular. Só eventos de dia inteiro.
//
// Segurança: o texto do usuário (título, descrição) passa por `escapar`, que remove quebras de linha e
// escapa os separadores do formato. Assim um título como "x\r\nBEGIN:VEVENT..." não consegue criar
// campos ou eventos falsos no arquivo.
namespace IcsUtil {

struct Evento {
    std::string uid;        // identificador estável (o mesmo item gera sempre o mesmo UID)
    std::string titulo;
    std::string descricao;
    int ano = 0, mes = 0, dia = 0;                 // primeiro dia
    int anoFim = 0, mesFim = 0, diaFim = 0;        // último dia INCLUSIVO (0 = evento de um dia só)
};

// Texto -> valor de campo do iCalendar: tira CR/LF (viram espaço), escapa \ ; , e demais controles.
inline std::string escapar(const std::string &texto)
{
    std::string saida;
    saida.reserve(texto.size());
    for (std::size_t i = 0; i < texto.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(texto[i]);
        switch (c) {
        case '\\': saida += "\\\\"; break;
        case ';': saida += "\\;"; break;
        case ',': saida += "\\,"; break;
        case '\r':
            if (i + 1 < texto.size() && texto[i + 1] == '\n')
                ++i;
            saida += ' ';
            break;
        case '\n': saida += ' '; break;
        default:
            if (c < 0x20 || c == 0x7F)
                saida += ' ';  // outros caracteres de controle
            else
                saida += static_cast<char>(c);
        }
    }
    return saida;
}

// Dobra linhas com mais de 75 bytes (o formato exige), sem cortar no meio de um caractere UTF-8.
inline std::string dobrarLinha(const std::string &linha)
{
    constexpr std::size_t kLimite = 75;
    if (linha.size() <= kLimite)
        return linha + "\r\n";
    std::string saida;
    std::size_t inicio = 0;
    bool primeira = true;
    while (inicio < linha.size()) {
        std::size_t tamanho = primeira ? kLimite : kLimite - 1;  // a continuação começa com 1 espaço
        if (inicio + tamanho >= linha.size()) {
            tamanho = linha.size() - inicio;
        } else {
            while (tamanho > 0 && (static_cast<unsigned char>(linha[inicio + tamanho]) & 0xC0) == 0x80)
                --tamanho;  // não parte um caractere UTF-8 ao meio
        }
        if (!primeira)
            saida += ' ';
        saida.append(linha, inicio, tamanho);
        saida += "\r\n";
        inicio += tamanho;
        primeira = false;
    }
    return saida;
}

inline std::string data8(int a, int m, int d)
{
    char buf[16];
    std::snprintf(buf, sizeof buf, "%04d%02d%02d", a, m, d);
    return buf;
}

// Dia seguinte (o DTEND de eventos de dia inteiro é exclusivo).
inline void diaSeguinte(int &a, int &m, int &d)
{
    static const int dias[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool bissexto = (a % 4 == 0 && a % 100 != 0) || a % 400 == 0;
    const int limite = (m == 2 && bissexto) ? 29 : dias[m - 1];
    if (++d > limite) {
        d = 1;
        if (++m > 12) {
            m = 1;
            ++a;
        }
    }
}

// `carimbo`: "20261002T120000Z" (quando o arquivo foi gerado); `nome`: nome do calendário.
inline std::string montar(const std::vector<Evento> &eventos, const std::string &nome, const std::string &carimbo)
{
    std::string s;
    s += dobrarLinha("BEGIN:VCALENDAR");
    s += dobrarLinha("VERSION:2.0");
    s += dobrarLinha("PRODID:-//Caderno+//Calendario//PT-BR");
    s += dobrarLinha("CALSCALE:GREGORIAN");
    s += dobrarLinha("METHOD:PUBLISH");
    s += dobrarLinha("X-WR-CALNAME:" + escapar(nome));
    for (const Evento &e : eventos) {
        int fa = e.anoFim ? e.anoFim : e.ano;
        int fm = e.anoFim ? e.mesFim : e.mes;
        int fd = e.anoFim ? e.diaFim : e.dia;
        diaSeguinte(fa, fm, fd);
        s += dobrarLinha("BEGIN:VEVENT");
        s += dobrarLinha("UID:" + escapar(e.uid));
        s += dobrarLinha("DTSTAMP:" + carimbo);
        s += dobrarLinha("DTSTART;VALUE=DATE:" + data8(e.ano, e.mes, e.dia));
        s += dobrarLinha("DTEND;VALUE=DATE:" + data8(fa, fm, fd));
        s += dobrarLinha("SUMMARY:" + escapar(e.titulo));
        if (!e.descricao.empty())
            s += dobrarLinha("DESCRIPTION:" + escapar(e.descricao));
        s += dobrarLinha("TRANSP:TRANSPARENT");
        s += dobrarLinha("END:VEVENT");
    }
    s += dobrarLinha("END:VCALENDAR");
    return s;
}

}  // namespace IcsUtil
