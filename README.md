# Professor Organizado

App desktop (Qt 6 Widgets + SQLite + QXlsx) para professores organizarem a vida escolar.
Tudo offline: os dados ficam num único arquivo SQLite no computador.

| Seção | O que faz |
|---|---|
| 🏠 Hoje | Aulas do dia (com AGORA/PRÓXIMA), tarefas pendentes, provas dos próximos 14 dias |
| 👥 Turmas | Turmas e alunos; abas **Arquivos** (apresentações/documentos) e **Anotações** da turma |
| 📊 Notas | Planilha editável, média ponderada automática, importar/exportar Excel |
| 📋 Frequência | Chamada do dia (P/F/J/A) e resumo do mês em grade |
| 🗓️ Horário | Grade semanal com cores por turma, arrastar e soltar |
| 📚 Aulas | Plano de aula (tema, objetivos, materiais) + apresentações anexadas |
| 📝 Anotações | Texto rico com tags, ligado a turma/aluno/aula; salva sozinho |
| ✅ Tarefas | Lista completa com prazo, prioridade e filtros |
| 📅 Calendário | Provas, feriados, reuniões, prazos de tarefas e avaliações |
| 📈 Relatórios | Gráficos de desempenho e PDFs (boletim, frequência, ficha do aluno) |
| 🔍 Busca (Ctrl+K) | Procura em tudo, ignorando acentos e maiúsculas |
| 💾 Backup | Automático a cada 24 h; salvar cópia; restaurar |

## Compilar e rodar no Windows com Qt Creator

1. **Instale o Qt 6** (Qt Online Installer, qt.io): marque *Qt 6.x → MinGW 64-bit* (ou MSVC 2019/2022 64-bit)
   e, em *Developer and Designer Tools*, **Qt Creator**, **CMake** e **Ninja**.
   O módulo **Qt SQL** (driver SQLite) já vem incluído. Não é preciso instalar Qt Charts nem PrintSupport.
2. **Baixar o QXlsx:** acesse https://github.com/QtExcel/QXlsx → **Code → Download ZIP**,
   extraia e coloque a pasta em `third_party/`, de modo que exista este arquivo:
   ```
   ProfOrganizer/third_party/QXlsx/QXlsx/CMakeLists.txt
   ```
   (Com git: `git clone https://github.com/QtExcel/QXlsx third_party/QXlsx`.)
3. Abra o Qt Creator → **File → Open File or Project…** → selecione `CMakeLists.txt` desta pasta.
4. Na tela *Configure Project*, marque o Kit do Qt 6 (ex.: *Desktop Qt 6.x MinGW 64-bit*) → **Configure Project**.
5. **Build → Build Project** (Ctrl+B) e depois **Run** (Ctrl+R).
6. Os dados ficam em `%APPDATA%\ProfOrganizer\ProfOrganizer\professor.db` e o esquema é
   atualizado sozinho (migrações) quando você abre uma versão nova do programa.

### Build na nuvem (sem instalar nada no seu PC)
O arquivo `.github/workflows/build-windows.yml` compila o projeto no GitHub (Windows + MSVC + Qt 6.8), roda os
testes e gera uma pasta pronta para usar. Passo a passo:
1. Crie um repositório **privado** no GitHub e envie esta pasta (sem precisar incluir `third_party/`; o workflow baixa o QXlsx):
   ```
   git init
   git add .
   git commit -m "Professor Organizado"
   git branch -M main
   git remote add origin https://github.com/SEU-USUARIO/SEU-REPOSITORIO.git
   git push -u origin main
   ```
2. No GitHub, abra a aba **Actions** → **Build Windows**. O build roda sozinho (leva ~10–15 min na primeira vez).
3. Se der **erro de compilação**, abra o passo "Compilar", copie as linhas com `error` e me mande.
4. Se der certo, role até **Artifacts** no fim da página da execução e baixe **ProfOrganizer-build-N-xxxxxxx** (N = número da execução, xxxxxxx = início do commit; o mesmo código aparece na barra de título do app).
5. Descompacte o `.zip` em qualquer pasta e execute `ProfOrganizer.exe` (não precisa instalar o Qt).

### Testes
```
ctest --test-dir build --output-on-failure
```
Três testes sem Qt: `test_media` (média ponderada), `test_horario` (contas da grade) e
`test_frequencia` (percentual de frequência). No Qt Creator: Build → Run CTest.

### Distribuir para outro PC (opcional)
Em *Qt 6.x (MinGW) Command Prompt*, dentro da pasta do `.exe` de Release: `windeployqt ProfOrganizer.exe`

## Guia rápido

### Notas
Escolha a turma, crie avaliações em **+ Avaliação** (peso, nota máxima, período) e digite as notas nas células
(`8,5` ou `8.5`). Grava na hora. `Delete` apaga, `Ctrl+C`/`Ctrl+V` copiam blocos de/para o Excel.
- **Média:** cada nota é normalizada para 0–10 (`nota / máximo × 10`) e combinada pelo peso; só entram
  avaliações com nota lançada. Médias abaixo da nota de corte ficam em vermelho.
- **Excel:** exporta a turma inteira com a média como fórmula. Importa por matrícula ou nome, sem criar
  alunos e sem apagar notas existentes; uma pré-visualização deixa escolher o destino de cada coluna.

### Frequência
- **P** presente · **F** falta · **J** falta justificada · **A** atraso.
- Atraso conta como presença; falta justificada **não** reduz a frequência; só as chamadas registradas entram na conta.
- Frequência abaixo de 75% fica em vermelho. "Marcar todos como presentes" não altera quem já tem registro.
- Dias marcados como feriado/recesso no calendário geram um aviso na chamada.

### Horário
Arraste o bloco para mudar dia/horário (de 5 em 5 min); arraste a borda de baixo para mudar a duração; duplo clique
no vazio cria aula, no bloco edita; botão direito abre o menu. Duas aulas não ocupam o mesmo horário no mesmo dia.

### Aulas e arquivos
Cada plano de aula aceita anexos (PowerPoint, PDF, etc.) que abrem no **programa padrão do sistema**. O programa
guarda só o **caminho** do arquivo: se você mover ou apagar o original, o anexo aparece como "não encontrado".
Por segurança, executáveis e scripts (`.exe`, `.bat`, `.js`…) não são anexados nem abertos.

### Anotações
Formatação (negrito, itálico, sublinhado, marca-texto, listas), tags separadas por vírgula e vínculo opcional com
turma, aluno e aula. Salva sozinha ~1 s depois de parar de digitar e ao trocar de anotação ou de tela.

### Relatórios
Gráficos: média por aluno, média por avaliação, distribuição das médias, frequência por aluno e evolução de um aluno
(dá para salvar como PNG). PDFs: boletim da turma, frequência da turma e ficha individual do aluno.

### Backup e restauração
- Backup automático ao abrir (se o último tem mais de 24 h), a cada hora de uso e ao fechar (se tem mais de 12 h).
  Ficam em `%APPDATA%\ProfOrganizer\ProfOrganizer\backups`; os 10 mais recentes são mantidos.
- O backup guarda **os dados**, não os arquivos anexados (esses continuam onde estão).
- **Restaurar** valida o arquivo e o aplica na **próxima abertura** do programa. Os dados atuais não são apagados:
  ficam ao lado como `professor.db.antes-da-restauracao-<data>`.
- Dica: use "Salvar uma cópia em…" para guardar backups em pen drive ou pasta sincronizada na nuvem.

## Arquitetura
- `src/core`     — lógica pura sem Qt (média, frequência, horário, texto, anexos), testável
- `src/models`   — structs de dados
- `src/database` — conexão, migrações versionadas e repositórios (**único lugar com SQL**); `Repositorios.h` agrupa todos
- `src/services` — `.xlsx`, PDF, backup, importação de notas, agregação de desempenho
- `src/ui`       — widgets; recebem os repositórios por injeção (`main.cpp` → `MainWindow` → telas)

### Evoluir o esquema
Adicione uma nova `Migracao` (versao + 1) no fim de `Migrations::todas()` em `src/database/Migrations.cpp`
(as versões 2, 3 e 4 são exemplos). Nunca edite migrações já distribuídas. Um backup feito por uma versão mais nova
do programa é recusado na restauração.

## Atalhos
`Ctrl+1…9`, `Ctrl+0` seções · `Ctrl+K` busca global · na planilha de notas: `Ctrl+C`, `Ctrl+V`, `Delete`, `F2`.

## Limitações conhecidas
- A chamada é por **dia** (uma por turma/dia), não por aula: duas aulas da mesma turma no mesmo dia compartilham a chamada.
- A frequência considera todas as chamadas do ano; o filtro de período vale só para notas.
- A busca global não indexa o conteúdo dos arquivos anexados, apenas nome e turma.
- Os PDFs são gerados a partir de HTML simples (`QTextDocument`); o layout é funcional, não sofisticado.
