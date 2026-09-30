#pragma once

#include "models/Horario.h"

#include <QList>
#include <QString>
#include <optional>

// Repositório do horário semanal (tabela "horarios"). Só lista aulas de
// turmas não arquivadas.
class HorarioRepository {
public:
    QList<Horario> listar();  // ordenado por dia e horário de início
    std::optional<Horario> buscar(int id);

    int inserir(const Horario &horario);  // devolve o novo id, ou 0 em caso de erro
    bool atualizar(const Horario &horario);
    bool remover(int id);

    // Já existe outra aula naquele dia que se sobrepõe ao intervalo?
    // (um professor não dá duas aulas ao mesmo tempo). `ignorarId` = a própria aula ao editar.
    bool temConflito(int diaSemana, const QTime &inicio, const QTime &fim, int ignorarId = 0);

    QString ultimoErro() const { return m_erro; }

private:
    QString m_erro;
};
