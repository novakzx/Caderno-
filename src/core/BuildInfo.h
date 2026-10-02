#pragma once

#include <QString>

// Identificação da versão em execução. O número de versão vem do projeto; o
// "build" é o início do hash do commit, injetado pelo workflow do GitHub Actions
// (PROFORG_BUILD_ID_RAW). Em compilações locais aparece "local".
//
// Serve para saber, sem dúvida, qual .exe a pessoa está rodando ao investigar um erro.
#define PROFORG_STR_INNER(x) #x
#define PROFORG_STR(x) PROFORG_STR_INNER(x)

// Vem do CMake (project(... VERSION ...)); o valor abaixo só vale em compilações fora dele.
#ifndef PROFORG_VERSION
#define PROFORG_VERSION "1.1.0"
#endif

inline QString versaoDoApp()
{
    return QStringLiteral(PROFORG_VERSION);
}

// Autoria do programa: um só lugar. Aparece em "Sobre o Caderno+", na tela de entrada, no instalador
// e nas propriedades do .exe (resources/app.rc.in e installer/setup/recursos/setup.rc repetem o texto).
inline QString autorDoApp()
{
    return QStringLiteral("Gabriel (novakzx)");
}

inline QString enderecoDoProjeto()
{
    return QStringLiteral("https://github.com/novakzx/Caderno-");
}

// Passo a passo para ligar o Assistente de IA (abre no navegador; o endereço é fixo, nunca vem de dados do usuário).
inline QString guiaDaIa()
{
    return enderecoDoProjeto() + QStringLiteral("/blob/main/docs/GUIA-IA.md");
}

inline QString identificacaoDoBuild()
{
#ifdef PROFORG_BUILD_ID_RAW
    return QStringLiteral("%1 · build %2").arg(versaoDoApp(), QLatin1String(PROFORG_STR(PROFORG_BUILD_ID_RAW)));
#else
    return QStringLiteral("%1 · build local").arg(versaoDoApp());
#endif
}
