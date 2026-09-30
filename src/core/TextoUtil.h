#pragma once

#include <QChar>
#include <QString>

// Normaliza texto para comparações tolerantes: sem acentos, minúsculo e com
// espaços simplificados. Ex.: "  João  DA Silva " -> "joao da silva".
// Usado para casar nomes de alunos/colunas em planilhas importadas.
inline QString normalizarTexto(const QString &texto)
{
    const QString decomposto = texto.normalized(QString::NormalizationForm_D);
    QString saida;
    saida.reserve(decomposto.size());
    for (const QChar c : decomposto) {
        if (c.category() != QChar::Mark_NonSpacing)  // descarta os acentos soltos
            saida.append(c);
    }
    return saida.simplified().toLower();
}
