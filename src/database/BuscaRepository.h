#pragma once

#include <QDate>
#include <QList>
#include <QString>

enum class TipoBusca { Turma, Aluno, Anotacao, Aula, Tarefa, Evento, Anexo };

// Um item pesquisável (qualquer registro do programa que a busca global encontra).
struct ItemBusca {
    TipoBusca tipo = TipoBusca::Turma;
    int id = 0;
    int turmaId = 0;             // turma relacionada (0 = nenhuma), para navegar até ela
    QString titulo;
    QString subtitulo;           // contexto mostrado abaixo do título
    QString textoNormalizado;    // tudo o que é pesquisável, sem acentos e em minúsculas
    QString caminho;             // só para anexos: caminho do arquivo
    QDate data;                  // data relacionada (aula, tarefa, evento), se houver
};

// Monta o índice da busca global. O volume de dados de um professor é pequeno
// (milhares de registros), então o índice é carregado de uma vez ao abrir a busca
// e filtrado em memória: assim a busca ignora acentos e maiúsculas, o que o
// LIKE do SQLite não faz.
class BuscaRepository {
public:
    QList<ItemBusca> carregarIndice();

    QString ultimoErro() const { return m_erro; }

private:
    QString m_erro;
};
