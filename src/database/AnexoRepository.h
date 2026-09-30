#pragma once

#include "models/Anexo.h"

#include <QList>
#include <QString>
#include <optional>

// Repositório de anexos (tabela "anexos"): guarda só o caminho dos arquivos.
class AnexoRepository {
public:
    // Anexos de uma aula específica.
    QList<Anexo> listarPorAula(int aulaId);
    // Todos os anexos da turma, inclusive os das aulas dela.
    QList<Anexo> listarPorTurma(int turmaId);
    std::optional<Anexo> buscar(int id);

    int inserir(const Anexo &anexo);  // devolve o novo id, ou 0 em caso de erro
    bool remover(int id);             // remove só o vínculo, nunca o arquivo

    QString ultimoErro() const { return m_erro; }

private:
    QList<Anexo> consultar(const QString &where, int valor);

    QString m_erro;
};
