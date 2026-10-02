#pragma once

#include "models/Aluno.h"

#include <QDialog>

class OcorrenciaRepository;
class QComboBox;
class QDateEdit;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

// Ocorrências de UM aluno: linha do tempo (elogio, conduta, dificuldade, contato com a família...)
// com lista, formulário de cadastro/edição e exclusão. Tudo é gravado na hora.
class OcorrenciasDialog : public QDialog {
    Q_OBJECT
public:
    OcorrenciasDialog(OcorrenciaRepository &repo, const Aluno &aluno, QWidget *parent = nullptr);

private:
    void recarregar(int selecionarId = 0);
    int ocorrenciaSelecionadaId() const;
    void carregarNoFormulario();
    void limparFormulario();
    void salvar();
    void excluir();
    void atualizarEstado();

    OcorrenciaRepository &m_repo;
    Aluno m_aluno;
    int m_editandoId = 0;  // 0 = cadastrando uma nova

    QTableWidget *m_tabela = nullptr;
    QLabel *m_vazio = nullptr;
    QDateEdit *m_data = nullptr;
    QComboBox *m_tipo = nullptr;
    QPlainTextEdit *m_texto = nullptr;
    QPushButton *m_btnNova = nullptr;
    QPushButton *m_btnSalvar = nullptr;
    QPushButton *m_btnExcluir = nullptr;
};
