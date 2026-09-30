# Guia: automação com o Google Classroom (e outras integrações)

Este guia explica **como** ligar o Caderno+ ao Google Classroom. A integração não está implementada no programa: o texto
mostra o caminho completo, do cadastro no Google até o código, para você decidir se quer fazê-la.

## 1. O que dá para automatizar

A API do Classroom (`https://classroom.googleapis.com/v1`) permite, para um professor:

| Tarefa | Direção | Dificuldade |
| --- | --- | --- |
| Importar as **turmas** (cursos) do Classroom para o Caderno+ | Classroom → Caderno+ | fácil |
| Importar os **alunos** de cada turma (nome e e-mail) | Classroom → Caderno+ | fácil |
| Importar **atividades** (`courseWork`) como avaliações | Classroom → Caderno+ | média |
| Importar **notas** das entregas (`studentSubmissions`) | Classroom → Caderno+ | média |
| Criar atividade no Classroom a partir do Caderno+ | Caderno+ → Classroom | média |
| Enviar **notas** de volta para o Classroom | Caderno+ → Classroom | difícil (regra abaixo) |
| Publicar **avisos** (`announcements`) | Caderno+ → Classroom | fácil |

> **Regra importante da API:** só o projeto do Google Cloud que **criou** uma atividade pode alterá-la ou lançar nota
> nela. Atividades criadas pelo professor no site do Classroom não aceitam nota enviada pelo seu aplicativo. Por isso o
> caminho seguro é começar **só importando** (leitura) e deixar o envio de notas para depois.

## 2. Passo a passo no Google

1. Acesse o **Google Cloud Console** (console.cloud.google.com) e crie um projeto (ex.: "Caderno+").
2. Em **APIs e serviços → Biblioteca**, ative a **Google Classroom API**.
3. Em **Tela de consentimento OAuth**:
   - **Interno**, se a escola usa Google Workspace e o administrador permitir (não exige verificação do Google);
   - **Externo**, para contas pessoais. Em modo "Teste" só funcionam os e-mails que você listar (até 100) e a
     autorização expira em 7 dias. Para liberar a todos, o Google exige **verificação do aplicativo** quando há escopos
     sensíveis (os do Classroom são).
4. Escopos (peça só o necessário, começando pelos de leitura):
   - `https://www.googleapis.com/auth/classroom.courses.readonly`
   - `https://www.googleapis.com/auth/classroom.rosters.readonly`
   - `https://www.googleapis.com/auth/classroom.profile.emails` (e-mail dos alunos)
   - `https://www.googleapis.com/auth/classroom.coursework.students.readonly` (atividades e notas, leitura)
   - para escrever depois: `classroom.coursework.students` e `classroom.announcements`
5. Em **Credenciais → Criar credenciais → ID do cliente OAuth**, tipo **App para computador (Desktop)**. Guarde o
   `client_id` (e o `client_secret`; em apps desktop ele não é realmente secreto).

## 3. Como funciona o login (OAuth 2.0 em app desktop)

O programa abre o navegador, o professor autoriza, e o Google devolve um código para um endereço local
(`http://127.0.0.1:<porta>`), que o programa troca por um *access token* (dura ~1 h) e um *refresh token* (longo).

No Qt isso é o módulo **Qt Network Authorization**:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Svg Network NetworkAuth)
target_link_libraries(ProfOrganizer PRIVATE Qt6::Network Qt6::NetworkAuth)
```

```cpp
auto *fluxo = new QOAuth2AuthorizationCodeFlow(this);
fluxo->setAuthorizationUrl(QUrl("https://accounts.google.com/o/oauth2/v2/auth"));
fluxo->setAccessTokenUrl(QUrl("https://oauth2.googleapis.com/token"));
fluxo->setClientIdentifier(CLIENT_ID);
fluxo->setRequestedScopeTokens({"https://www.googleapis.com/auth/classroom.courses.readonly", /* ... */});
auto *resposta = new QOAuthHttpServerReplyHandler(QHostAddress::LocalHost, 0, this);  // porta livre
fluxo->setReplyHandler(resposta);
connect(fluxo, &QAbstractOAuth::authorizeWithBrowser, &QDesktopServices::openUrl);
connect(fluxo, &QAbstractOAuth::granted, this, [=] { /* já pode chamar a API */ });
fluxo->grant();
```

Use **PKCE** (no Qt 6.8: `QOAuth2AuthorizationCodeFlow::setPkceMethod(S256)`) e peça `access_type=offline` para receber
o refresh token.

**Onde guardar o refresh token:** nunca no banco do Caderno+ nem em `QSettings` em texto. Use o **Gerenciador de
Credenciais do Windows** (biblioteca `QtKeychain`). Quem tem o refresh token acessa o Classroom do professor.

## 4. Chamadas de exemplo

```text
GET  /v1/courses?teacherId=me&courseStates=ACTIVE            -> turmas do professor
GET  /v1/courses/{id}/students                               -> alunos (nome, e-mail, id)
GET  /v1/courses/{id}/courseWork                             -> atividades
GET  /v1/courses/{id}/courseWork/{cw}/studentSubmissions     -> entregas e notas (assignedGrade)
POST /v1/courses/{id}/courseWork                             -> cria atividade
PATCH /v1/courses/{id}/courseWork/{cw}/studentSubmissions/{s}?updateMask=draftGrade,assignedGrade
POST /v1/courses/{id}/announcements                          -> aviso
```

As listas vêm paginadas (`nextPageToken`): continue pedindo até acabar. Trate `429` (limite de uso) esperando e
tentando de novo, e `403` (sem permissão) com uma mensagem clara.

## 5. Como encaixar no Caderno+

Siga a arquitetura do projeto (camadas, sem SQL em widget):

1. **Migração nova** (a de número seguinte, nunca editar as antigas) com colunas para ligar os dois mundos:
   `turmas.classroom_id`, `alunos.classroom_id` (e `alunos.email` já existe), `avaliacoes.classroom_id`.
2. `src/services/ClassroomService.{h,cpp}`: `QNetworkAccessManager` + `QJsonDocument`; devolve structs simples
   (`CursoClassroom`, `AlunoClassroom`...), sem tocar no banco.
3. Um **caso de uso** em `services/` que cruza os dados: para cada curso, cria/atualiza a turma; casa os alunos pelo
   **e-mail** (mais confiável que o nome) e, se não achar, oferece criar. Nunca apagar nada vindo do Classroom.
4. Tela `ui/ClassroomDialog`: "Conectar", lista de cursos com caixas de seleção, resumo do que será importado
   ("12 alunos novos, 3 atualizados") e **confirmação antes de gravar**, como já faz a importação de Excel.
5. Testes: lógica de casamento de alunos em `tests/` (pura), e o JSON da API com respostas de exemplo gravadas em arquivo.

## 6. Cuidados

- **LGPD**: dados de alunos saem do Google para o computador do professor. Confirme com a escola, use só o necessário
  e deixe claro na tela de login do Google/Classroom quais permissões o programa pede.
- **Funciona offline?** Depois de importar, sim. A integração é uma ação do usuário ("Sincronizar"), não um serviço
  que fica rodando em segundo plano.
- **Contas escolares** podem ter a API bloqueada pelo administrador. Teste antes com a conta real.
- **Verificação do Google** pode levar semanas se o app for para muitos professores; para uso próprio ou de uma escola
  (tipo "Interno"), não precisa.

## 7. Atalho sem programar

Se só quiser os dados no Excel: no Classroom, a aba **Notas** tem "Exportar notas para Planilhas Google / CSV". Baixe
como `.xlsx` e use **Notas → Importar Excel** no Caderno+, que já existe. Serve de ponte até a integração existir.

## 8. Outras automações possíveis ("e etc")

| Ideia | Como | Esforço |
| --- | --- | --- |
| **Calendário** (celular, Google Agenda, Outlook) | exportar eventos e provas em arquivo **.ics** (formato de texto simples; não precisa de internet) | baixo |
| **Boletim por e-mail/WhatsApp** | gerar o PDF (já existe) e abrir o app de e-mail com o arquivo; envio automático exige servidor e consentimento dos responsáveis | médio |
| **Importar de sistema da escola** (SIGA, SGE, Sponte...) | quase nenhum tem API aberta: use a exportação CSV/Excel deles com o importador | baixo |
| **Backup na nuvem** | copiar a pasta `backups` para o OneDrive/Google Drive (a pasta sincronizada do usuário) | baixo |
| **Lembretes** | notificação do Windows (`QSystemTrayIcon::showMessage`) para aulas e prazos | baixo |
