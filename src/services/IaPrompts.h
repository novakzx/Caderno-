#pragma once

#include "services/IaService.h"

#include <QList>
#include <QString>

// Monta os pedidos para a IA a partir do que o professor preencheu. Só entra no pedido o que a pessoa digitou
// nos campos do Assistente: o programa NUNCA anexa dados do banco (nomes de alunos, notas, frequência...).
namespace IaPrompts {

enum class Tarefa { PlanoDeAula, Questoes, Atividade, Comunicado, AdaptarTexto, Livre };

struct Pedido {
    Tarefa tarefa = Tarefa::PlanoDeAula;
    QString disciplina;
    QString serie;      // ex.: "8º ano do Ensino Fundamental"
    QString tema;
    QString duracao;    // ex.: "50 minutos"
    QString detalhes;   // observações livres, ou o texto a adaptar / a pergunta (no modo livre)
    int quantidade = 5; // nº de questões / atividades
};

QString rotulo(Tarefa tarefa);
// Dica do campo de texto livre para cada tarefa.
QString dicaDosDetalhes(Tarefa tarefa);
// Quais campos a tarefa usa (os outros ficam escondidos na tela).
bool usaDisciplina(Tarefa tarefa);
bool usaTema(Tarefa tarefa);
bool usaDuracao(Tarefa tarefa);
bool usaQuantidade(Tarefa tarefa);

// Remove caracteres de controle e limita o tamanho (o texto vai para um serviço externo).
QString limpar(const QString &texto, int limite);

// Mensagens prontas para IaService::enviar (system + user).
QList<MensagemIa> montar(const Pedido &pedido);

// O texto inclui, ao final, o aviso de que foi gerado por IA e precisa de revisão?
QString avisoDeRevisao();

}  // namespace IaPrompts
