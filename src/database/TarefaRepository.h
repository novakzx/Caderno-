#pragma once

#include "models/Tarefa.h"

#include <QDate>
#include <QList>
#include <QPair>
#include <QString>
#include <QVariant>
#include <optional>

// Repositório de tarefas (tabela "tarefas").
class TarefaRepository {
public:
    enum class Filtro { Pendentes, Concluidas, Todas };

    // Pendentes: atrasadas primeiro, depois por prazo e prioridade; sem prazo por último.
    QList<Tarefa> listar(Filtro filtro = Filtro::Pendentes, int turmaId = 0, int limite = 1000);
    QList<Tarefa> listarPendentes(int limite = 50) { return listar(Filtro::Pendentes, 0, limite); }
    // Tarefas (qualquer situação) com prazo entre as datas; usado pelo calendário.
    QList<Tarefa> listarComPrazo(const QDate &de, const QDate &ate);
    std::optional<Tarefa> buscar(int id);

    int inserir(const Tarefa &tarefa);  // devolve o novo id, ou 0 em caso de erro
    bool atualizar(const Tarefa &tarefa);
    bool marcarConcluida(int id, bool concluida);
    bool remover(int id);

    QString ultimoErro() const { return m_erro; }

private:
    QList<Tarefa> consultar(const QString &where, const QList<QPair<QString, QVariant>> &parametros,
                            const QString &ordem, int limite);

    QString m_erro;
};
