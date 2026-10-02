#pragma once

#include "services/ImportadorAlunos.h"

#include <QDialog>

class AlunoRepository;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QTabWidget;

// Importar alunos para uma turma: o professor escolhe um arquivo (.csv, .txt, .xlsx) ou cola uma lista,
// vê o que será feito linha a linha (novos, já existentes, repetidos, inválidos) e só então confirma.
class ImportarAlunosDialog : public QDialog {
    Q_OBJECT
public:
    ImportarAlunosDialog(AlunoRepository &alunos, int turmaId, const QString &turmaNome, QWidget *parent = nullptr);

    int alunosCriados() const { return m_criados; }

private:
    void escolherArquivo();
    void baixarModelo();
    void analisarTextoColado();
    void mostrarTabela(const std::optional<QList<QStringList>> &tabela, const QString &erro);
    void preencherPrevia();
    void importar();

    AlunoRepository &m_alunos;
    ImportadorAlunos m_importador;
    int m_turmaId = 0;
    QList<QStringList> m_tabela;
    PlanoImportacaoAlunos m_plano;
    int m_criados = 0;

    QTabWidget *m_origem = nullptr;
    QLabel *m_arquivo = nullptr;
    QPlainTextEdit *m_colado = nullptr;
    QLabel *m_resumo = nullptr;
    QTableWidget *m_previa = nullptr;
    QPushButton *m_botaoImportar = nullptr;
};
