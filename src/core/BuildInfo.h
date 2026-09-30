#pragma once

#include <QString>

// Identificação da versão em execução. O número de versão vem do projeto; o
// "build" é o início do hash do commit, injetado pelo workflow do GitHub Actions
// (PROFORG_BUILD_ID_RAW). Em compilações locais aparece "local".
//
// Serve para saber, sem dúvida, qual .exe a pessoa está rodando ao investigar um erro.
#define PROFORG_STR_INNER(x) #x
#define PROFORG_STR(x) PROFORG_STR_INNER(x)

inline QString identificacaoDoBuild()
{
#ifdef PROFORG_BUILD_ID_RAW
    return QStringLiteral("1.0.0 · build %1").arg(QLatin1String(PROFORG_STR(PROFORG_BUILD_ID_RAW)));
#else
    return QStringLiteral("1.0.0 · build local");
#endif
}
