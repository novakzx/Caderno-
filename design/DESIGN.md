# Caderno+

Design system do Caderno+, o app para professores organizarem aulas, turmas e planejamento.

## Princípios

- **Calmo e organizado.** O professor usa o app por muito tempo, entre uma aula e outra. Fundo de papel quente (`surface-100`), muito espaço, pouca cor. A cor aparece só quando significa algo.
- **Uma cor de ação.** `primary` (verde-lousa) marca a ação principal, o item ativo e o foco. Uma ação primária por tela.
- **Destaque com lápis.** `accent` (ocre) sinaliza o que pede atenção — uma pendência, uma anotação — como um grifo, nunca como texto no tema claro.
- **Bordas, não sombras.** Cartões em `surface-200` com borda `line` de 1px. Nada de gradientes.
- **Estado nunca só por cor.** `success`, `warning` e `danger` vêm sempre com ícone ou rótulo.

## Tom de voz

Português claro, direto e gentil, na segunda pessoa ("Sua aula de amanhã"). Verbos no botão: "Salvar plano", "Adicionar turma" — nunca "OK" ou "Enviar". Mensagens de erro dizem o que fazer: "Escolha um horário para a aula."

## Fundamentos visuais

- **Tipografia:** Figtree em tudo (arquivos em `fonts/`, pesos 400, 600 e 700). `title` para o título da tela, `heading` para cartões e seções, `body` para leitura, `small` para metadados em `ink-muted`, `label` para rótulos de campo e cabeçalhos de tabela.
- **Espaçamento:** grade de 4px. `space-4` dentro de cartões, `space-6`/`space-8` entre blocos.
- **Raios:** `radius-md` em botões, inputs e cartões de aula; `radius-lg` em painéis e modais; `radius-sm` em chips.
- **Temas:** claro e escuro com os mesmos nomes de token — nunca use hex direto no código.
- **Acessibilidade:** texto ≥4.5:1 nos dois temas; bordas de controle em `line-strong` (3:1); foco com anel `focus` de 2px.

## Cores de turma

Cada turma tem uma cor (campo `cor` da tabela `turma`). Sugira as seis `turma-1`…`turma-6` no seletor; todas aceitam texto `on-turma` com contraste 5:1+ nos dois temas. A cor reforça, o nome identifica: blocos do Horário e chips sempre mostram o nome da turma.

## Frequência

**P** presente (`success`) · **F** falta (`danger`) · **J** falta justificada (`warning`) · **A** atraso (ocre suave, conta como presença). A letra aparece sempre; a cor é só reforço.

## Logo

O símbolo é uma página de caderno: um quadrado verde-lousa (`primary`, cantos de 14/64) com três pautas em papel (`surface-100`) e um **+** ocre no lugar do fim da última linha. A assinatura junta o símbolo ao nome **Caderno+** em Figtree Bold, com o **+** em ocre.

- `caderno-logo.svg` em fundos claros; `caderno-logo-dark.svg` em fundos escuros; `caderno-mark.svg` sozinho para ícone do app, favicon e avatar.
- O símbolo é igual nos dois temas. Área livre mínima: metade da altura do símbolo em volta. Tamanho mínimo: 16px (símbolo), 24px de altura (assinatura).
- O nome é sempre escrito **Caderno+**, sem espaço antes do +. Não recolorir, girar ou separar o + do nome.

## Iconografia

Ícones próprios de traço 1.5px em grade de 24 (componente `Icon`), 20px na navegação e 16px em metadados, na cor do texto ao lado. Um ícone por seção do app (hoje, turmas, notas, frequência, horário, aulas, anotações, tarefas, calendário, relatórios) mais utilitários (busca, mais, sala, anexo, check, alerta). Emojis não entram na interface.

## Componentes

| Componente | Uso no app |
| --- | --- |
| `Sidebar` | Navegação das dez seções (`#sidebar`) |
| `PageHeader` | Título, subtítulo e ações de cada página (`#pageTitle`, `#pageSubtitle`) |
| `Button` | `primary` (uma por tela), `secondary`, `ghost`, `danger` (`#primary`, `#danger`) |
| `TextField` | Campos com rótulo, dica e erro |
| `Badge` | AGORA, PRÓXIMA e estados (`#badge`) |
| `ClassChip` | Turma com sua cor, em listas e filtros |
| `Card` | Contêiner padrão (`#card`) |
| `LessonCard` | Aula do dia no Hoje e em Aulas |
| `Tabs` | Abas de Turmas e Frequência |
| `AttendanceMark` | Chamada P/F/J/A |
| `WeekSchedule` | Grade semanal do Horário |

## Aplicando no app Qt

O app desktop (Qt 6) monta as cores no `ThemeManager.cpp` (struct `Paleta`). Correspondência: `fundo` → `surface-100`, `superficie` → `surface-200`, `superficieAlt`/`hover` → `surface-300`, `borda` → `line`, `texto` → `ink`, `textoSuave` → `ink-muted`, `destaque` → `primary`, `destaqueTexto` → `on-primary`, `selecao` → `primary-soft`, `barraLateral` → `surface-200`, `barraLateralTexto` → `ink-muted`. Os vermelhos e verdes fixos espalhados no código (`#D64545`, `#2E9E5B`, `#E08A1E`, `#B8860B`) passam a ser `danger`, `success`, `warning` e `accent`. Para usar a Figtree no desktop, empacote os arquivos TTF com o app; sem eles, o fallback é Segoe UI.

## Ainda falta

Os ícones como arquivos SVG soltos, para usar fora do React (hoje estão no componente `Icon`).
