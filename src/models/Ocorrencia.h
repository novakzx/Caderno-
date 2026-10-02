#pragma once

#include <QDate>
#include <QString>

// Modelo de dados: um registro sobre um aluno (elogio, conduta, dificuldade, contato com a família...).
// `tipo` é o id de OcorrenciaUtil::kTipos (texto guardado no banco).
struct Ocorrencia {
    int id = 0;
    int alunoId = 0;
    QDate data;
    QString tipo = QStringLiteral("outro");
    QString texto;
};
