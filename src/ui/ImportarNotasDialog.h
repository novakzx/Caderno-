#pragma once

#include "models/Avaliacao.h"
#include "services/ImportadorNotas.h"

#include <QDialog>

class QTableWidget;

// Tela de pré-visualização da importação: mostra o que será feito com cada
// coluna da planilha e deixa o professor ajustar antes de gravar.
class ImportarNotasDialog : public QDialog {
    Q_OBJECT
public:
    ImportarNotasDialog(const QList<Avaliacao> &avaliacoesDaTurma,
                        const XlsxService::Planilha &planilha, const PlanoImportacao &plano,
                        QWidget *parent = nullptr);

    // Plano com as escolhas feitas pelo usuário (colunas marcadas e destinos).
    PlanoImportacao plano() const;

private:
    PlanoImportacao m_plano;
    QTableWidget *m_tabela = nullptr;
};
