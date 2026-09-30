#pragma once

#include <QDate>
#include <QMetaType>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

// Pequenos auxiliares para montar parâmetros de consultas SQL.

// Liga um parâmetro a uma consulta.
//
// POR QUE EXISTE: no Qt, um QString "nulo" (QString(), o valor de um campo de texto
// que nunca foi preenchido) é gravado como NULL no SQLite. Isso quebra de duas formas:
//   - colunas "TEXT NOT NULL DEFAULT ''" recusam a gravação ("NOT NULL constraint failed");
//   - comparações como  `:tag = ''`  viram NULL (nunca verdadeiras), e o filtro
//     "sem filtro" passa a não devolver nenhuma linha.
// Aqui todo texto entra como texto (no mínimo vazio). Para gravar NULL de propósito,
// passe um QVariant() inválido (veja nuloSeZero e dataOuNulo): esse continua sendo NULL.
inline void ligar(QSqlQuery &q, const QString &nome, const QVariant &valor)
{
    if (valor.metaType().id() == QMetaType::QString && valor.isNull())
        q.bindValue(nome, QVariant(QStringLiteral("")));  // vazio, mas NÃO nulo
    else
        q.bindValue(nome, valor);
}

// NULL de propósito: um QVariant inválido é ligado como NULL pelo Qt.
inline QVariant valorNulo()
{
    return QVariant();
}

// Chave estrangeira opcional: 0 (sem vínculo) vira NULL no banco.
inline QVariant nuloSeZero(int id)
{
    return id > 0 ? QVariant(id) : valorNulo();
}

// Data inválida vira NULL; válida vira texto ISO (yyyy-MM-dd).
inline QVariant dataOuNulo(const QDate &d)
{
    return d.isValid() ? QVariant(d.toString(Qt::ISODate)) : valorNulo();
}

// Lê uma data ISO do banco (aceita "yyyy-MM-dd" e "yyyy-MM-dd HH:mm:ss").
inline QDate lerData(const QVariant &v)
{
    return QDate::fromString(v.toString().left(10), Qt::ISODate);
}
