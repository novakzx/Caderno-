#pragma once

#include "services/DesempenhoService.h"

#include <QString>

// Geração de relatórios em PDF: primeiro monta um texto HTML (tabelas simples)
// e depois o "imprime" num arquivo PDF com QPdfWriter + QTextDocument.
// Só Qt Gui: não precisa do módulo PrintSupport.
namespace RelatorioPdf {

// Boletim da turma: notas de cada aluno, média e frequência.
QString htmlBoletim(const Boletim &boletim, double notaCorte);

// Frequência da turma: presenças, faltas, justificadas, atrasos e percentual por aluno.
QString htmlFrequencia(const Boletim &boletim);

// Ficha individual: notas, média, frequência e ocorrências (faltas/atrasos).
QString htmlFicha(const FichaAluno &ficha, double notaCorte);

// Grava o HTML em PDF (A4). Retorna false e preenche *erro em caso de falha.
bool salvarPdf(const QString &html, const QString &caminho, const QString &titulo, bool paisagem, QString *erro);

}  // namespace RelatorioPdf
