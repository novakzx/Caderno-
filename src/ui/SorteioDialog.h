#pragma once

#include "core/SorteioUtil.h"
#include "models/Aluno.h"

#include <QDialog>
#include <QList>
#include <memory>
#include <random>

class FrequenciaRepository;
class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTabWidget;
class QTimer;

// Ferramentas de sala de aula: sortear um aluno (sem repetir até todos saírem) e montar grupos equilibrados.
// Só mexe na memória: não grava nada no banco.
class SorteioDialog : public QDialog {
    Q_OBJECT
public:
    SorteioDialog(const QList<Aluno> &alunosAtivos, FrequenciaRepository &frequencia, int turmaId, const QString &turmaNome,
                  QWidget *parent = nullptr);

private:
    QList<Aluno> participantes() const;  // alunos ativos, e só os presentes se a opção estiver marcada
    void sortearAluno();
    void passoDaAnimacao();
    void montarGrupos();
    void copiarGrupos();
    void reiniciarFila();
    void atualizarContagem();

    QList<Aluno> m_alunos;
    FrequenciaRepository &m_frequencia;
    int m_turmaId = 0;
    std::mt19937 m_rng;

    QTabWidget *m_abas = nullptr;
    QCheckBox *m_soPresentes = nullptr;
    QLabel *m_aviso = nullptr;

    // Sorteio
    QLabel *m_nomeSorteado = nullptr;
    QLabel *m_contagem = nullptr;
    QPushButton *m_botaoSortear = nullptr;
    QCheckBox *m_semRepeticao = nullptr;
    QTimer *m_animacao = nullptr;
    std::unique_ptr<SorteioUtil::SorteioSemRepeticao> m_fila;
    QList<Aluno> m_filaDeAlunos;  // participantes quando a fila foi criada
    int m_passos = 0;
    int m_totalDePassos = 0;
    int m_escolhido = -1;

    // Grupos
    QRadioButton *m_porQuantidade = nullptr;
    QRadioButton *m_porTamanho = nullptr;
    QSpinBox *m_numero = nullptr;
    QPlainTextEdit *m_resultadoDosGrupos = nullptr;
    QPushButton *m_botaoCopiar = nullptr;
};
