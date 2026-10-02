#pragma once

// Agrupa todos os repositórios para passá-los de uma vez às telas, em vez de
// uma lista crescente de parâmetros. Criado em main.cpp (os repositórios são
// leves e compartilham a conexão padrão do banco).
class TurmaRepository;
class AlunoRepository;
class AvaliacaoRepository;
class NotaRepository;
class HorarioRepository;
class TarefaRepository;
class AgendaRepository;
class AulaRepository;
class AnexoRepository;
class AnotacaoRepository;
class FrequenciaRepository;
class EventoRepository;
class BuscaRepository;
class OcorrenciaRepository;

struct Repositorios {
    TurmaRepository &turmas;
    AlunoRepository &alunos;
    AvaliacaoRepository &avaliacoes;
    NotaRepository &notas;
    HorarioRepository &horarios;
    TarefaRepository &tarefas;
    AgendaRepository &agenda;
    AulaRepository &aulas;
    AnexoRepository &anexos;
    AnotacaoRepository &anotacoes;
    FrequenciaRepository &frequencia;
    EventoRepository &eventos;
    BuscaRepository &busca;
    OcorrenciaRepository &ocorrencias;
};
