#include "services/IaPrompts.h"

namespace IaPrompts {

namespace {

constexpr int kLimiteCurto = 200;
constexpr int kLimiteDetalhes = 4000;

QString sistema()
{
    return QStringLiteral(
        "Você é um assistente pedagógico para professores da educação básica brasileira. "
        "Responda SEMPRE em português do Brasil, com linguagem clara e direta, organizada em títulos e tópicos curtos. "
        "Não invente fatos, datas, estatísticas, citações nem códigos da BNCC: se não tiver certeza, diga que o professor "
        "deve conferir. Não peça nem use dados pessoais de alunos; se o texto trouxer nomes de crianças, trate-os como "
        "\"o aluno\". Seja prático: o professor vai usar sua resposta em sala de aula.");
}

QString ou(const QString &valor, const QString &padrao)
{
    return valor.trimmed().isEmpty() ? padrao : valor.trimmed();
}

}  // namespace

QString rotulo(Tarefa tarefa)
{
    switch (tarefa) {
    case Tarefa::PlanoDeAula: return QStringLiteral("Plano de aula");
    case Tarefa::Questoes: return QStringLiteral("Questões de prova");
    case Tarefa::Atividade: return QStringLiteral("Atividades e dinâmicas");
    case Tarefa::Comunicado: return QStringLiteral("Comunicado aos responsáveis");
    case Tarefa::AdaptarTexto: return QStringLiteral("Adaptar um texto");
    case Tarefa::Livre: return QStringLiteral("Pergunta livre");
    }
    return QString();
}

QString dicaDosDetalhes(Tarefa tarefa)
{
    switch (tarefa) {
    case Tarefa::PlanoDeAula: return QStringLiteral("Opcional: recursos que você tem, dificuldades da turma, estilo de aula...");
    case Tarefa::Questoes: return QStringLiteral("Opcional: tipo (múltipla escolha, discursiva), nível de dificuldade, habilidades...");
    case Tarefa::Atividade: return QStringLiteral("Opcional: tamanho da turma, tempo, materiais, objetivo da atividade...");
    case Tarefa::Comunicado: return QStringLiteral("O que precisa ser comunicado? (ex.: reunião dia 15, passeio, tarefa de casa). Evite nomes de alunos.");
    case Tarefa::AdaptarTexto: return QStringLiteral("Cole aqui o texto e diga como adaptar (ex.: mais simples para o 6º ano, resumir em 10 linhas).");
    case Tarefa::Livre: return QStringLiteral("Escreva sua pergunta ou pedido para a IA.");
    }
    return QString();
}

bool usaDisciplina(Tarefa t) { return t == Tarefa::PlanoDeAula || t == Tarefa::Questoes || t == Tarefa::Atividade || t == Tarefa::AdaptarTexto; }
bool usaTema(Tarefa t) { return t == Tarefa::PlanoDeAula || t == Tarefa::Questoes || t == Tarefa::Atividade; }
bool usaDuracao(Tarefa t) { return t == Tarefa::PlanoDeAula || t == Tarefa::Atividade; }
bool usaQuantidade(Tarefa t) { return t == Tarefa::Questoes || t == Tarefa::Atividade; }

QString limpar(const QString &texto, int limite)
{
    QString saida;
    saida.reserve(qMin<int>(texto.size(), limite));
    for (const QChar c : texto) {
        if (c == QLatin1Char('\n') || c == QLatin1Char('\t'))
            saida.append(c);
        else if (c == QLatin1Char('\r'))
            continue;
        else if (c.unicode() < 0x20 || c.unicode() == 0x7F || c.category() == QChar::Other_Format)
            continue;  // controles e caracteres invisíveis
        else
            saida.append(c);
        if (saida.size() >= limite)
            break;
    }
    return saida.trimmed();
}

QString avisoDeRevisao()
{
    return QStringLiteral("Conteúdo gerado por IA. Revise antes de usar com a turma.");
}

QList<MensagemIa> montar(const Pedido &pedido)
{
    const QString disciplina = ou(limpar(pedido.disciplina, kLimiteCurto), QStringLiteral("a disciplina"));
    const QString serie = ou(limpar(pedido.serie, kLimiteCurto), QStringLiteral("a turma"));
    const QString tema = ou(limpar(pedido.tema, kLimiteCurto), QStringLiteral("(tema a critério da IA, coerente com a série)"));
    const QString duracao = ou(limpar(pedido.duracao, kLimiteCurto), QStringLiteral("50 minutos"));
    const QString detalhes = limpar(pedido.detalhes, kLimiteDetalhes);
    const int quantidade = qBound(1, pedido.quantidade, 20);

    QString usuario;
    switch (pedido.tarefa) {
    case Tarefa::PlanoDeAula:
        usuario = QStringLiteral(
            "Crie um plano de aula de %1 de %2 para %3, sobre o tema: %4.\n"
            "Organize em: 1) Objetivos de aprendizagem; 2) Materiais; 3) Desenvolvimento passo a passo, com o tempo de cada etapa; "
            "4) Atividade de fixação; 5) Como verificar a aprendizagem; 6) Adaptações para alunos com dificuldade.")
                      .arg(duracao, disciplina, serie, tema);
        break;
    case Tarefa::Questoes:
        usuario = QStringLiteral(
            "Crie %1 questões de %2 para %3, sobre o tema: %4.\n"
            "Para cada questão traga o enunciado e, ao final, um gabarito com a resposta correta e uma explicação curta. "
            "Varie a dificuldade, do mais fácil ao mais difícil.")
                      .arg(quantidade).arg(disciplina, serie, tema);
        break;
    case Tarefa::Atividade:
        usuario = QStringLiteral(
            "Sugira %1 atividades ou dinâmicas de sala de aula de %2 para %3, sobre o tema: %4, cada uma com cerca de %5. "
            "Para cada uma descreva: objetivo, materiais, passo a passo e como avaliar.")
                      .arg(quantidade).arg(disciplina, serie, tema, duracao);
        break;
    case Tarefa::Comunicado:
        usuario = QStringLiteral(
            "Escreva um comunicado curto e cordial da escola para os pais ou responsáveis dos alunos de %1. "
            "Use tom respeitoso e objetivo, com saudação e despedida, sem inventar datas ou horários além dos informados. "
            "Assunto: ")
                      .arg(serie);
        break;
    case Tarefa::AdaptarTexto:
        usuario = QStringLiteral("Considerando a disciplina de %1 e o público %2, faça o seguinte com o texto abaixo: ").arg(disciplina, serie);
        break;
    case Tarefa::Livre:
        usuario = QString();
        break;
    }

    if (!detalhes.isEmpty()) {
        if (pedido.tarefa == Tarefa::Livre || pedido.tarefa == Tarefa::Comunicado || pedido.tarefa == Tarefa::AdaptarTexto)
            usuario += (usuario.isEmpty() ? QString() : QStringLiteral("\n")) + detalhes;
        else
            usuario += QStringLiteral("\n\nObservações do professor: ") + detalhes;
    }
    if (usuario.trimmed().isEmpty())
        usuario = QStringLiteral("Olá! Como você pode me ajudar nas minhas aulas?");

    return {{QStringLiteral("system"), sistema()}, {QStringLiteral("user"), usuario}};
}

}  // namespace IaPrompts
