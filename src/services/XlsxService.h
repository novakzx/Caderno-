#pragma once

#include "models/Aluno.h"
#include "models/Avaliacao.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

// Leitura e escrita de arquivos .xlsx (via QXlsx). Esta camada só conhece
// arquivos: não acessa o banco de dados nem widgets.
//
// Formato da planilha (idêntico na exportação e na importação):
//
//        |  A         |  B      |  C        |  D         | ...  |  última
//   1    | Matrícula  | Aluno   | Prova 1   | Trabalho   | ...  | Média
//   2    |            | Peso    |   2       |   1        | ...  |
//   3    |            | Nota máxima | 10    |   5        | ...  |
//   4    |            | Período |   1       |   1        | ...  |
//   5..  | 2024001    | Ana     |   8,5     |   4        | ...  | (fórmula)
//
// As linhas 2-4 (peso, nota máxima, período) são opcionais na importação.
// A coluna "Média" é ignorada ao importar (é sempre recalculada pelo programa).
namespace XlsxService {

// ------------------------------- Exportação -------------------------------

struct DadosExportacao {
    QString titulo;                   // vira o nome da aba
    QList<Aluno> alunos;
    QList<Avaliacao> avaliacoes;      // na ordem das colunas
    QHash<qint64, double> notas;      // chave = NotaRepository::chave(avaliacaoId, alunoId)
};

bool exportar(const QString &caminho, const DadosExportacao &dados, QString *erro);

// ------------------------------- Importação -------------------------------

struct ColunaPlanilha {
    QString nome;
    double peso = 1.0;
    double notaMaxima = 10.0;
    int periodo = 1;
    bool temMetadados = false;  // peso/máximo/período vieram da planilha?
};

struct LinhaPlanilha {
    int linhaExcel = 0;
    QString matricula;
    QString nome;
    QVector<std::optional<double>> notas;  // uma posição por ColunaPlanilha (nullopt = vazio)
};

struct Planilha {
    QList<ColunaPlanilha> colunas;
    QList<LinhaPlanilha> linhas;
    QStringList avisos;  // ex.: "Linha 7, coluna 'Prova 1': valor 'abc' ignorado"
};

// Lê a primeira aba do arquivo. Em caso de erro devolve nullopt e preenche *erro.
std::optional<Planilha> importar(const QString &caminho, QString *erro);

// Lê a primeira aba como uma tabela de textos (uma QStringList por linha, sem linhas vazias).
// Datas viram "yyyy-MM-dd" e números inteiros (ex.: matrícula 2024001) não ganham ".0".
// Serve para listas simples, como a importação de alunos; mesmos limites de tamanho de importar().
std::optional<QList<QStringList>> lerTabela(const QString &caminho, QString *erro);

}  // namespace XlsxService
