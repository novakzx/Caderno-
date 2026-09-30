#pragma once

#include <QDate>
#include <QMetaType>
#include <QVariant>

// Pequenos auxiliares para montar parâmetros de consultas SQL.

// Chave estrangeira opcional: 0 (sem vínculo) vira NULL no banco.
inline QVariant nuloSeZero(int id)
{
    return id > 0 ? QVariant(id) : QVariant(QMetaType(QMetaType::Int));
}

// Data inválida vira NULL; válida vira texto ISO (yyyy-MM-dd).
inline QVariant dataOuNulo(const QDate &d)
{
    return d.isValid() ? QVariant(d.toString(Qt::ISODate)) : QVariant(QMetaType(QMetaType::QString));
}

// Lê uma data ISO do banco (aceita "yyyy-MM-dd" e "yyyy-MM-dd HH:mm:ss").
inline QDate lerData(const QVariant &v)
{
    return QDate::fromString(v.toString().left(10), Qt::ISODate);
}
