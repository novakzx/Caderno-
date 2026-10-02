#pragma once

#include "models/Aluno.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

class AlunoRepository;

// Uma linha da lista de alunos a importar, já analisada.
struct LinhaImportacaoAluno {
    enum class Situacao {
        Nova,               // vai ser criada
        JaExiste,           // já há esse aluno na turma (mesma matrícula, ou mesmo nome sem matrícula): ignorada
        RepetidaNoArquivo,  // aparece de novo na própria lista: só a primeira vale
        Invalida,           // falta o nome ou algum campo passa do limite: ignorada
    };

    int linhaOrigem = 0;  // número da linha no arquivo (a 1ª é a do cabeçalho, se houver)
    Aluno aluno;          // turmaId e id ficam em branco até gravar
    Situacao situacao = Situacao::Nova;
    QString detalhe;      // motivo de ignorar, ou aviso (ex.: "e-mail inválido ignorado")
};

struct PlanoImportacaoAlunos {
    QList<LinhaImportacaoAluno> linhas;
    QString erro;  // não vazio = a lista não pode ser importada (ex.: não achei a coluna de nomes)
    bool temCabecalho = false;

    int total(LinhaImportacaoAluno::Situacao s) const
    {
        int n = 0;
        for (const LinhaImportacaoAluno &l : linhas)
            n += l.situacao == s ? 1 : 0;
        return n;
    }
    int novos() const { return total(LinhaImportacaoAluno::Situacao::Nova); }
};

// Importação de alunos a partir de CSV/TXT (separado por ; , ou tabulação), Excel (.xlsx) ou texto colado.
//
// Regras (sem nenhuma interface gráfica):
//  - a 1ª linha deve ter os títulos: Nome (obrigatório), Matrícula, E-mail, Nascimento, Observações
//    (nomes parecidos também valem: "Aluno", "RA", "E-mail", "Data de nascimento"...). Uma lista de uma
//    coluna só, sem título, é lida como lista de nomes;
//  - nada é gravado antes de a pessoa ver o plano (planejar) e confirmar (executar);
//  - quem já está na turma não é duplicado; só alunos novos são criados, todos numa única transação;
//  - e-mail ou data de nascimento inválidos não derrubam a linha: o campo é ignorado, com aviso;
//  - o texto vira só dado (nada é executado nem interpretado como fórmula).
class ImportadorAlunos {
public:
    static constexpr int kMaximoDeLinhas = 5000;
    static constexpr qint64 kTamanhoMaximoDoTexto = 5LL * 1024 * 1024;

    explicit ImportadorAlunos(AlunoRepository &alunos) : m_alunos(alunos) {}

    // Texto (arquivo .csv/.txt ou colagem) -> tabela. Aceita UTF-8 (com ou sem BOM) e Windows-1252.
    static std::optional<QList<QStringList>> tabelaDeTexto(const QByteArray &bytes, QString *erro);
    // .csv/.txt/.tsv ou .xlsx -> tabela.
    static std::optional<QList<QStringList>> tabelaDeArquivo(const QString &caminho, QString *erro);

    PlanoImportacaoAlunos planejar(int turmaId, const QList<QStringList> &tabela);
    bool executar(int turmaId, const PlanoImportacaoAlunos &plano, int *criados, QString *erro);

    // Conteúdo de um arquivo-modelo (CSV em UTF-8 com BOM, que o Excel abre com os acentos certos).
    static QByteArray modeloCsv();

private:
    AlunoRepository &m_alunos;
};
