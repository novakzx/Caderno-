# Caderno+

App desktop (Qt 6 Widgets + SQLite + QXlsx) para professores organizarem a vida escolar.
Tudo offline: os dados ficam num único arquivo SQLite no computador.

| Seção | O que faz |
|---|---|
| 🏠 Hoje | Aulas do dia (com AGORA/PRÓXIMA), tarefas pendentes, **alunos em atenção**, provas dos próximos 14 dias |
| 👥 Turmas | Turmas e alunos; **importar lista (CSV/Excel/colar)**, **ocorrências** por aluno; abas **Arquivos** e **Anotações** da turma |
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
| 🔔 Lembretes | Notificações do Windows: aula prestes a começar, tarefas e provas de hoje/amanhã |
| ✨ Assistente | IA gratuita (Cloudflare Workers AI): plano de aula, questões, atividades, comunicados (`docs/GUIA-IA.md`) |
| 🎲 Sorteio | Sortear aluno e montar grupos equilibrados (em Turmas) |
| ⚙️ Configurações | Bloqueio por inatividade, lembretes, aviso de versão nova e IA |

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
   git commit -m "Caderno+"
   git branch -M main
   git remote add origin https://github.com/SEU-USUARIO/SEU-REPOSITORIO.git
   git push -u origin main
   ```
2. No GitHub, abra a aba **Actions** → **Build Windows**. O build roda sozinho (leva ~10–15 min na primeira vez).
3. Se der **erro de compilação**, abra o passo "Compilar", copie as linhas com `error` e me mande.
4. Se der certo, role até **Artifacts** no fim da página da execução. Há dois: **Instalador-build-N-xxxxxxx** (o `Caderno-Setup-...exe`, um
   `.zip` portátil e o `SHA256SUMS.txt`) e **ProfOrganizer-build-N-xxxxxxx** (só a pasta do programa). N = número da execução,
   xxxxxxx = início do commit; o mesmo código aparece na barra de título do app.
5. Para instalar: descompacte o artefato do instalador e execute `Caderno-Setup-...exe` (não pede administrador, cria atalho no Menu Iniciar
   e não apaga seus dados ao desinstalar). Ou use o `.zip` portátil: descompacte e execute `ProfOrganizer.exe` (não precisa instalar o Qt).

**Lançar uma versão (Release pública):** aumente `VERSION` em `CMakeLists.txt` (é a única fonte do número), faça commit e crie uma etiqueta
`vX.Y.Z` igual a essa versão (`git tag v1.1.0 && git push origin v1.1.0`). O workflow cria a Release no GitHub com o instalador, o `.zip` e os hashes.
Detalhes e o aviso do SmartScreen em `docs/GUIA-PUBLICAR.md`.

### Testes
```
ctest --test-dir build --output-on-failure
```
Nove testes sem Qt: `test_media`, `test_horario`, `test_frequencia`, `test_contraste`, `test_conta`, `test_atencao` (alunos em atenção e tipos
de ocorrência), `test_lembrete` (quando avisar), `test_csv` (leitor de CSV) e `test_utilitarios` (versões, sorteios e calendário .ics). O autoteste (`ProfOrganizer.exe --selftest relatorio.txt`) exercita
também o banco, as migrações, os repositórios, as contas, a importação e os lembretes. No Qt Creator: Build → Run CTest.

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

### Alunos em atenção e ocorrências
- O painel **Hoje** lista os alunos que pedem atenção. **Urgente**: média abaixo da nota de corte (a mesma da tela de Notas) ou frequência
  abaixo de 75%. **Atenção**: média até 0,5 acima do corte, frequência até 5 pontos acima de 75%, ou 2+ ocorrências negativas (conduta,
  dificuldade) nos últimos 30 dias. Sem notas ou sem chamadas registradas, não há alerta. Clique no nome para abrir o aluno na turma.
- Em **Turmas → Alunos → Ocorrências…** registre elogio, conduta, dificuldade de aprendizagem, contato com a família ou outro, com data e descrição.
  A tabela mostra quantas cada aluno tem, e a **ficha em PDF** inclui a lista.
- Novos tipos de ocorrência: acrescente uma linha em `src/core/OcorrenciaUtil.h` (não precisa de migração).

### Importar alunos
**Turmas → Importar lista…** aceita `.csv`, `.txt`, `.xlsx` ou texto colado (por exemplo, copiado do Excel). A primeira linha traz os títulos:
**Nome** (obrigatório), Matrícula, E-mail, Nascimento (aceita "Aluno", "RA", "Data de nascimento"...). Uma lista de uma coluna só, sem títulos, vale como
lista de nomes. Use **Baixar modelo** para ver o formato. Você vê tudo antes de gravar: quem já está na turma (mesma matrícula, ou mesmo nome sem matrícula)
e as linhas repetidas ou inválidas são ignoradas; e-mail ou data inválidos viram aviso. Limites: 5 MB (texto) e 5.000 linhas.

### Assistente de IA
A tela **Assistente** gera textos para a aula com o Cloudflare Workers AI (gratuito até 10.000 "neurons" por dia). Você cria uma conta gratuita e
cola o Account ID e um token em **Configurações > Assistente de IA**; o passo a passo está em `docs/GUIA-IA.md`. Só o texto que você escreve nos campos
é enviado (nunca dados do banco), a chave fica protegida pelo Windows, e toda resposta deve ser revisada antes do uso.

### Sorteio e grupos
Em **Turmas > Sortear…**: sorteia um aluno (sem repetir até todos saírem, opcionalmente só os presentes hoje) ou divide a turma em grupos equilibrados
(por quantidade de grupos ou de alunos por grupo), com botão de copiar.

### Calendário no celular
Em **Calendário > Exportar .ics…** o programa grava um arquivo com eventos, provas, prazos de tarefas pendentes e avaliações datadas, que abre no
Google Agenda, Outlook e no calendário do celular.

### Segurança e atualizações
Em **Configurações** você escolhe o bloqueio por inatividade (a janela volta ao login depois de N minutos parada) e se quer o aviso de versão nova
(consulta o GitHub no máximo uma vez por dia; nunca baixa nem instala nada sozinho).

### Lembretes
O botão **Lembretes** (barra lateral) liga as notificações do Windows: aula prestes a começar (5 a 30 min antes, ou desligado), tarefas e provas de hoje e de
amanhã (a partir das 8h; tarefa atrasada não avisa, ela já aparece em vermelho no Hoje). Cada aviso aparece uma vez por dia. **Só funciona com o Caderno+
aberto**; o ícone na área de notificação abre o programa. Se nada aparecer, veja o "Assistente de foco" do Windows (há um botão de teste).

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
- `src/core`     — lógica pura sem Qt (média, frequência, horário, texto, anexos, atenção, ocorrências, lembretes, CSV), testável
- `src/models`   — structs de dados
- `src/database` — conexão, migrações versionadas e repositórios (**único lugar com SQL**); `Repositorios.h` agrupa todos
- `src/services` — `.xlsx`, PDF, backup, importação de notas e de alunos, agregação de desempenho, lembretes
- `src/ui`       — widgets; recebem os repositórios por injeção (`main.cpp` → `MainWindow` → telas)

### Evoluir o esquema
Adicione uma nova `Migracao` (versao + 1) no fim de `Migrations::todas()` em `src/database/Migrations.cpp`
(as versões 2 a 6 são exemplos). Nunca edite migrações já distribuídas. Um backup feito por uma versão mais nova
do programa é recusado na restauração.

## Atalhos
`Ctrl+1…9`, `Ctrl+0` seções · `Ctrl+K` busca global · `F1` Sobre o Caderno+ · na planilha de notas: `Ctrl+C`, `Ctrl+V`, `Delete`, `F2`.

## Autoria
Caderno+ é feito por **Gabriel (novakzx)**. O nome aparece em "Sobre o Caderno+" (rodapé da janela ou `F1`), na tela de entrada, no instalador
e nas propriedades do `.exe`. Para mudar o texto, edite `autorDoApp()` em `src/core/BuildInfo.h`, `resources/app.rc.in`,
`installer/setup/src/instalar.h` (`kAutor`) e `installer/setup/recursos/setup.rc`.

## Limitações conhecidas
- A chamada é por **dia** (uma por turma/dia), não por aula: duas aulas da mesma turma no mesmo dia compartilham a chamada.
- A frequência considera todas as chamadas do ano; o filtro de período vale só para notas.
- A busca global não indexa o conteúdo dos arquivos anexados, apenas nome e turma.
- Os PDFs são gerados a partir de HTML simples (`QTextDocument`); o layout é funcional, não sofisticado.
