# Caderno+ (antigo Professor Organizado)

App desktop para professores organizarem turmas, notas, frequência, horário, aulas, anotações, tarefas e calendário.
Qt 6 Widgets + SQLite + QXlsx, C++17, CMake. Tudo offline. Textos da interface em português do Brasil.

## Build e testes
- Windows/Qt Creator: abrir `CMakeLists.txt` com um kit Qt 6 (precisa de `third_party/QXlsx`, ver README).
- Linha de comando: `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`.
- CI: `.github/workflows/build-windows.yml` (MSVC + Qt 6.8) compila, roda os testes e o autoteste (`ProfOrganizer.exe --selftest`).

## Arquitetura
- `src/core` lógica pura (testada em `tests/`), `src/models` structs, `src/database` único lugar com SQL (repositórios + `Migrations.cpp`),
  `src/services` xlsx/PDF/backup, `src/ui` widgets. Widgets recebem os repositórios por injeção (`Repositorios`), nunca abrem o banco.
- Mudança de esquema = nova migração em `Migrations.cpp`, nunca editar uma existente.

## NÃO mudar
- `setApplicationName`/`setOrganizationName("ProfOrganizer")`, o nome do alvo CMake e do .exe: definem a pasta de dados
  (`%APPDATA%\ProfOrganizer\ProfOrganizer\professor.db`) e as chaves do QSettings. Trocar apaga os dados do usuário na prática.
  O nome exibido é "Caderno+" (`setApplicationDisplayName`).

## Design system
- Fonte da verdade: `design/DESIGN.md` (guia: princípios, tom de voz, cores de turma, frequência, logo, componentes)
  e `design/tokens.json` (cores claro/escuro, tipografia, espaçamento, raios). Versão visual completa no artefato Caderno+ do Claude.
- Sempre usar os NOMES dos tokens; nunca hex solto em `.cpp`. O único lugar com hex é `src/core/Tokens.h` (espelho do `tokens.json`;
  ao mudar o json, atualize a tabela e rode `ctest`, que confere os contrastes em `tests/test_contraste.cpp`).
  Na interface: `ThemeManager::cor(Tokens::Id::Danger)`, `corHex(...)` (QSS/HTML), `corDaTurma(...)`, `definirEstado(label, Estado::Erro)`.
  Telas que guardam cor em itens/HTML refazem-se no sinal `ThemeManager::notificador().temaMudou`.
- Tema: `src/ui/ThemeManager.cpp` (QSS com marcadores `@{nome-do-token}`). Mapeamento: fundo→surface-100,
  superficie→surface-200, superficieAlt/hover→surface-300, borda→line (borda de CONTROLE, como campos, é line-strong), texto→ink,
  textoSuave→ink-muted, destaque→primary, destaqueTexto→on-primary, selecao→primary-soft, barraLateral→surface-200
  (com borda line à direita), barraLateralTexto→ink-muted. Item ativo da barra lateral: primary-soft + texto primary.
- Cor da turma: o banco guarda o NOME do token (`turma-1`…`turma-6`, padrão `turma-6`; migração 5); cor antiga em `#rrggbb` continua valendo.
- Estado nunca só por cor; texto ≥ 4.5:1 nos dois temas. Sem emojis na interface (tudo usa os ícones SVG de `resources/icons`).
- Ícones: `resources/icons/*.svg` (traço 1.5, grade 24, `currentColor`; recoloridos por `ThemeManager::icone`), embutidos pelo
  `resources/resources.qrc` (prefixo `:/icons`; o `app.ico` entra no .exe via `resources/app.rc.in`, que o CMake gera e que também leva as
  propriedades do .exe: versão, autor, copyright).
- Fonte: Figtree 400/600/700 em `resources/fonts` (prefixo `:/fonts`, licença OFL em `OFL.txt`), carregada por
  `ThemeManager::carregarFontes()`; reserva Segoe UI.

## Janela, animações, contas e segurança
- Janela principal **sem a moldura do sistema** (`Qt::FramelessWindowHint`): `ui/BarraDeTitulo` (arrastar, duplo clique maximiza, minimizar/
  maximizar/fechar) e redimensionar pelas bordas via filtro de eventos em `MainWindow`. Diálogos continuam com a moldura nativa.
- Animações: `ui/BotoesAnimados` (hover/seleção da barra lateral, desenhados à mão), `ui/PilhaAnimada` (fade ao trocar de seção),
  `LoginDialog` (fade de abertura, "tremida" no erro). Evitar animar telas pesadas por mais de ~200 ms.
- Ícones: todos em `resources/icons/*.svg` (traço 1.5, `currentColor`). Em botões use `ThemeManager::iconeNoBotao(...)` (acompanha o
  tema); setas de campos/calendário viram PNG de cache gerados por tema (`prepararImagensDoQss`). **Sem emojis nem setas de texto (◀ ▶).**
- Fonte do app é em **pixels** (`pointSize()` vale -1): nunca somar a `pointSize()`; tamanhos especiais via QSS por `objectName`.
  **Nunca pôr `font-family` com lista no QSS** (trava o Qt); a família vem de `QApplication::setFont`.
- Contas locais (`ContaService`, `ContaRepository`, `core/ContaUtil.h`): PBKDF2-SHA512, bloqueio por tentativas, código de recuperação.
  Cada conta tem o próprio banco (`DatabaseManager::caminhoAtual()`); `main.cpp` repete login → janela → "Sair da conta".
  Detalhes e riscos em `docs/SEGURANCA.md`.
- Regra de segurança: **texto do usuário nunca vira HTML, fórmula do Excel ou SQL** (`toHtmlEscaped`, `Qt::PlainText`,
  `escreverTexto` do XlsxService, parâmetros `:nome`). Anexos: `ehArquivoExecutavel` recusa executáveis, scripts e Office com macros.
## Alunos em atenção, ocorrências, lembretes e importação
- **Versão**: uma só fonte, `project(... VERSION x.y.z)` no `CMakeLists.txt` → `PROFORG_VERSION` → `versaoDoApp()` (`core/BuildInfo.h`).
  A etiqueta `vX.Y.Z` do Git precisa bater com ela (o workflow recusa se não bater).
- **Alunos em atenção**: regras puras em `core/AtencaoUtil.h` (limites, níveis Urgente/Atenção, motivos); `DesempenhoService::alunosEmAtencao`
  junta média + frequência + ocorrências; o cartão fica em `ui/HojePage`. A nota de corte é a do QSettings `"notaCorte"` (tela de Notas).
- **Ocorrências**: migração 6 (`ocorrencias`, sem CHECK em `tipo`), `OcorrenciaRepository`, tipos em `core/OcorrenciaUtil.h` (novo tipo = nova linha,
  sem migração; `negativa` decide se conta no painel), diálogo `ui/OcorrenciasDialog`, seção na ficha em PDF. Texto do usuário sempre escapado.
- **Lembretes**: regras em `core/LembreteUtil.h`, quem decide o que avisar é `services/LembreteService` (testável sem interface), e
  `ui/GerenteDeLembretes` mostra pela bandeja (`QSystemTrayIcon`) a cada 30 s e lembra o que já avisou no dia (QSettings, chave com resumo do e-mail).
  Só avisa com o programa aberto. Preferências em `ui/LembretesDialog` (QSettings `lembretes/...`).
- **Importar alunos**: `core/CsvUtil.h` (leitor de CSV puro), `services/ImportadorAlunos` (planejar → conferir → executar, numa transação),
  `XlsxService::lerTabela`, `ui/ImportarAlunosDialog`. Nada é gravado antes de a pessoa confirmar; limites de tamanho/linhas.
- **Instalador e Release**: `installer/setup` (instalador PRÓPRIO em Win32 + GDI+, sem Qt: `janela.cpp` desenha tudo à mão; `instalar.cpp` copia/registra/
  desinstala; `pacote.cpp` + `inflate.cpp` leem o pacote anexado ao .exe), `installer/empacotar.py` (monta o pacote) e `installer/testar-instalador.ps1`
  (teste de ponta a ponta, roda no CI). O job `publicar` só roda em etiqueta `v*` (com `contents: write` apenas nele). Não mudar o nome da chave de registro
  `CadernoPlus`, de `Desinstalar.exe` nem de `desinstalar.lst` (as versões instaladas dependem deles). Os atalhos de teste usam `--shortcuts-in`: nunca
  rode o teste com `--delete-data` (apagaria os dados reais).
- **IA**: `services/IaService` (Cloudflare Workers AI, SSE), `IaPrompts`, `SegredoService` (DPAPI), `ui/AssistentePage`, `ui/ConfiguracoesDialog`
  (4 abas: Segurança, Lembretes, Atualizações, IA). Nunca anexar dados do banco ao pedido; resposta sempre como texto simples. Guia: `docs/GUIA-IA.md`.
- **Outros**: `core/VersaoUtil.h` + `services/AtualizacaoService` (aviso de versão, opcional), `core/SorteioUtil.h` + `ui/SorteioDialog`,
  `core/IcsUtil.h` + `services/CalendarioExport` (.ics), bloqueio por inatividade em `MainWindow::verificarInatividade`.
  Preferências em QSettings (`services/Preferencias`). Roteiro de funções futuras: `docs/ROTEIRO.md`.
- Ao rodar o autoteste/teste do instalador na máquina de quem desenvolve, não apagar nada em `D:\toolchain` (a pasta é protegida): usar pastas temporárias.
- Janela principal com **mínimo 1100x680** (a tela Turmas não cabe em menos); a barra lateral cabe em 680 de altura, então ao acrescentar botões
  no rodapé confira isso (`D:\toolchain\shot2` tem uma checagem, fora do repositório).
- **Autoria e Sobre**: o autor tem uma fonte (`autorDoApp()` em `core/BuildInfo.h`; o texto se repete em `resources/app.rc.in`, `installer/setup/src/instalar.h`
  e `installer/setup/recursos/setup.rc`). `ui/SobreDialog` (F1 ou o link do rodapé da janela) mostra logo, versão, autor, projeto e licenças.
- **Ícone na barra de tarefas**: `ui/IconeDaJanela` entrega ao Windows o ícone embutido no .exe (recurso `IDI_ICON1`, 16 a 256 px) em `showEvent` da
  janela principal e do login; a janela sem moldura não pode depender só do ícone do Qt.
- **Estados vazios**: `ui/EstadoVazio::sobre(lista, icone, titulo, dica)` cobre lista/tabela vazia e some sozinho (liga ao modelo). Cabeçalhos de tabela
  alinham à esquerda (filtro em `ThemeManager.cpp`); coluna de números centralizada marca o cabeçalho com `setProperty("centralizado", true)`.
  Linhas dentro de cartões usam `objectName("linhaDoCartao")` (fundo transparente no QSS).
- Guias para o dono do projeto: `docs/GUIA-CLASSROOM.md` (integração/automações) e `docs/GUIA-PUBLICAR.md` (distribuição).
- Build local (se houver Qt): o relógio do Windows às vezes volta no tempo; se o Ninja disser "no work to do" sem motivo,
  rode `cmake --build <pasta> --target clean` e compile de novo.
