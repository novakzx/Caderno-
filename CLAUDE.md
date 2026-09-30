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
  `resources/resources.qrc` (prefixo `:/icons`; o `app.ico` entra no .exe via `resources/app.rc`).
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
- Guias para o dono do projeto: `docs/GUIA-CLASSROOM.md` (integração/automações) e `docs/GUIA-PUBLICAR.md` (distribuição).
- Build local (se houver Qt): o relógio do Windows às vezes volta no tempo; se o Ninja disser "no work to do" sem motivo,
  rode `cmake --build <pasta> --target clean` e compile de novo.
