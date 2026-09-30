#pragma once

#include <QString>

// Autoteste do programa, sem interface gráfica:
//
//     ProfOrganizer.exe --selftest relatorio.txt
//
// Abre um banco novo numa pasta temporária, aplica as migrações, exercita todos
// os repositórios (inserir, listar, filtrar, apagar em cascata...) e grava um
// relatório de texto em `arquivoSaida`. Se a abertura do banco falhar, roda
// também uma série de variantes para descobrir o que provoca a falha.
//
// É executado pelo GitHub Actions a cada build (veja .github/workflows), no mesmo
// Windows e com o mesmo Qt do .exe publicado. Não toca nos dados reais do usuário.
//
// Devolve 0 se tudo passou, 1 se algo falhou.
namespace AutoTeste {

int executar(const QString &arquivoSaida);

}  // namespace AutoTeste
