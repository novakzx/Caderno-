#pragma once

#include "models/Frequencia.h"

#include <QDate>
#include <QHash>
#include <QList>
#include <QString>

// Repositório da chamada (tabela "frequencia"): um registro por aluno e data.
class FrequenciaRepository {
public:
    // Registros de uma turma em um dia: alunoId -> registro.
    QHash<int, RegistroFrequencia> doDia(int turmaId, const QDate &data);

    // Grava (insere ou atualiza) a situação de um aluno em um dia.
    bool salvar(int alunoId, const QDate &data, QChar situacao, const QString &justificativa = QString());
    // Apaga o registro (volta a "sem registro").
    bool remover(int alunoId, const QDate &data);
    // Marca `situacao` para os alunos ATIVOS que ainda não têm registro no dia.
    // Quem já tem registro (ex.: uma falta) não é alterado.
    bool marcarRestantes(int turmaId, const QDate &data, QChar situacao);

    // Totais por aluno de uma turma. Datas inválidas = sem limite.
    QHash<int, ResumoFrequencia> resumoPorAluno(int turmaId, const QDate &de = QDate(),
                                                const QDate &ate = QDate());

    QList<RegistroFrequencia> doMes(int turmaId, int ano, int mes);
    QList<RegistroFrequencia> doAluno(int alunoId);  // do mais recente ao mais antigo

    QString ultimoErro() const { return m_erro; }

private:
    QString m_erro;
};
