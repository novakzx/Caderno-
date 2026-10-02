# Segurança do Caderno+

Revisão do código feita em outubro de 2026. Ela cobre o programa em si (C++/Qt/SQLite), as contas locais
(login), a importação/exportação de arquivos e a entrega (GitHub Actions). Não substitui uma auditoria externa.

## O que o programa guarda e onde

| Dado | Onde | Observação |
| --- | --- | --- |
| Turmas, alunos, notas, frequência, anotações | `%APPDATA%\ProfOrganizer\ProfOrganizer\professor.db` (1ª conta) e `contas\<id>\professor.db` (demais) | SQLite, **sem criptografia** |
| Contas (nome, e-mail, hash da senha) | `...\contas.db` | só hash PBKDF2 e sal, nunca a senha |
| Backups | pasta `backups` ao lado do banco de cada conta | cópia do banco, também sem criptografia |
| Ocorrências sobre alunos (conduta, dificuldade, contato com a família) | mesmo `professor.db` da conta | dado **sensível**: ver "Riscos" e "LGPD" |
| Preferências | registro do Windows (QSettings) | tema, última seção, último e-mail usado, lembretes (ligado, antecedência) e a lista de avisos já dados hoje (só ids e um resumo, sem nomes) |

Tudo fica no computador do usuário. O programa não envia nada pela internet.

## Achados e correções (já aplicadas)

| # | Gravidade | Achado | Situação |
| --- | --- | --- | --- |
| 1 | **Alta** | **Injeção de fórmula no Excel.** O QXlsx converte qualquer texto iniciado por `=` em fórmula. Um aluno chamado `=HYPERLINK("http://...","clique")` viraria fórmula ativa na planilha exportada. | **Corrigido**: nomes e matrículas entram sempre como texto (`escreverTexto`). Coberto no autoteste. |
| 2 | Média | **Anexos perigosos.** A lista de tipos bloqueados não incluía DLL, `.chm`, `.url`, imagens de disco (`.iso`, `.vhd`) nem Office com macros (`.xlsm`, `.docm`...). | **Corrigido**: lista ampliada. Coberto no autoteste. |
| 3 | Média | **HTML não escapado** nos nomes de alunos que vêm de planilha, no resumo da importação de notas. | **Corrigido**: nomes escapados. `HojePage::textoMudo` passou a usar texto simples. |
| 4 | Média | **Planilha gigante ou "bomba" de compressão** podia travar o programa na importação. | **Corrigido**: limite de 25 MB, 5.000 linhas e 200 colunas. |
| 5 | Média | **Banco vindo de fora.** Um backup restaurado de fonte não confiável poderia trazer views/gatilhos com funções SQL perigosas. | **Corrigido**: `PRAGMA trusted_schema = OFF` no banco e na validação do backup. A restauração já validava integridade, esquema e versão. |
| 6 | Baixa | **Dados apagados ficavam no arquivo** (um aluno excluído continuava legível nas páginas livres do SQLite). | **Corrigido**: `PRAGMA secure_delete = ON`. |
| 7 | Média | **Cadeia de entrega.** O CI usava ações do GitHub por rótulo móvel (`@v4`), baixava o QXlsx do `master` e o token tinha permissões padrão. | **Corrigido**: ações e QXlsx fixados por hash de commit, `permissions: contents: read`, `persist-credentials: false`. |
| 8 | Informativo | SQL: todas as consultas usam parâmetros; as partes montadas com texto (`SELECT_BASE`, `where`, `ordem`) vêm só de constantes do código. O `VACUUM INTO` do backup escapa as aspas do caminho. | Sem ação. |
| 9 | Média | **Importação de lista de alunos** (CSV/Excel/colagem) é entrada de fora. | **Tratada na construção**: limite de 5 MB (texto) e 5.000 linhas/200 colunas (Excel); tudo vira texto puro (nada é fórmula, HTML ou SQL; parâmetros `:nome`); nada é gravado antes da confirmação; gravação numa transação única; tamanhos de nome/matrícula/e-mail limitados. Coberto no autoteste (inclui texto `=HYPERLINK(...)`). |
| 10 | Baixa | **Ocorrências e PDF**: o texto da ocorrência entra na ficha em PDF (HTML interno). | **Escapado** com `toHtmlEscaped()`; coberto no autoteste. |
| 11 | Informativo | **Lembretes**: a notificação do Windows mostra título de tarefa/prova e nome da turma, que podem aparecer na tela bloqueada conforme a configuração do Windows. Nomes de alunos **não** entram nas notificações. | Sem ação; desligue em "Lembretes" se não quiser. |
| 12 | Média | **Instalador e Release**: a etiqueta publica um executável. | O job que publica é separado, com `contents: write` só nele; a versão da etiqueta precisa bater com o `CMakeLists.txt`; ações fixadas por hash; `SHA256SUMS.txt` na Release. O `.exe` continua **sem assinatura** (risco 4). |
| 13 | Média | **Instalador próprio** (`installer/setup`): lê um pacote anexado ao `.exe`, grava arquivos e apaga na desinstalação. | Todos os caminhos do pacote são conferidos **antes** de gravar (nada de `..`, unidade, `:` ou nome reservado); CRC-32 de cada arquivo; o desinstalador só apaga o que está na lista dele; a pasta de dados só é apagada com confirmação e se se chamar exatamente `ProfOrganizer`. Testado no CI com pacote corrompido e malicioso. |
| 14 | Média | **Chave da IA** (token do Cloudflare) guardada no computador. | Protegida pela **DPAPI do Windows** (só o mesmo usuário, no mesmo computador, abre); fora do Windows não é gravada. Validada (sem quebra de linha) e enviada só no cabeçalho `Authorization` por HTTPS; sem seguir redirecionamentos; nunca aparece em mensagens de erro. Não protege contra um programa malicioso rodando como o próprio usuário. |
| 15 | Média | **IA envia texto a um serviço externo** (Cloudflare). | Só vai o que a pessoa digita no Assistente, depois de um aviso de consentimento; nada do banco é anexado; a resposta é mostrada como **texto simples** e, ao salvar como anotação, é escapada. Veja `docs/GUIA-IA.md` e a seção LGPD. |
| 16 | Baixa | **Aviso de versão nova** consulta o GitHub. | Opcional (a pessoa escolhe); no máximo 1 vez por dia; não envia dados; o link recebido só é aceito se for da página de Releases do repositório (`..`, `%`, consulta e outros hosts são recusados); nada é baixado nem executado sozinho. |
| 17 | Baixa | **Computador compartilhado**: o programa ficava aberto. | **Bloqueio por inatividade** (Configurações): volta ao login depois de N minutos parado e fecha os diálogos abertos. |

## Contas locais (login)

- **Senha**: PBKDF2-HMAC-SHA512, 210.000 iterações, sal aleatório de 16 bytes; comparação em tempo constante.
- **Regras de senha**: 8 a 128 caracteres, letras e números (ou frase com 12+), fora de uma lista de senhas
  comuns, sem conter o e-mail ou o nome.
- **Tentativas erradas**: a partir da 5ª, a conta é bloqueada (30 s, dobrando até 15 min). A mensagem é a mesma para
  e-mail inexistente e senha errada, e o tempo de resposta também, então não dá para descobrir quem tem conta.
- **Recuperação**: código de 100 bits mostrado uma única vez; só o hash é guardado. Ao usá-lo, um código novo é gerado.
- **Cada conta tem o próprio arquivo de dados.** A primeira conta criada adota o `professor.db` que já existia.
- Os testes estão em `tests/test_conta.cpp` (regras) e no autoteste (`ContaService`, com relógio simulado).

## Riscos que continuam (decisões suas)

1. **O banco não é criptografado.** O login impede abrir o programa, mas quem tiver acesso ao arquivo `.db` (outro
   usuário do Windows com permissão, um backup copiado, o computador perdido sem BitLocker) consegue ler os dados.
   Mitigações: ligar o **BitLocker** (ou Criptografia de Dispositivo) no Windows; ou trocar o SQLite por **SQLCipher**
   (criptografia do arquivo com a senha da conta). Dá para fazer, mas aumenta o tamanho do projeto e a complexidade do build.
2. **Esqueceu a senha e perdeu o código de recuperação = os dados não são recuperáveis pelo programa.** O backup
   automático ajuda, mas ele também depende do mesmo arquivo.
3. **Anexos em pasta de rede** (`\\servidor\pasta`): abrir um arquivo de um servidor de terceiros pode enviar o hash de
   rede (NTLM) do usuário. Só anexe arquivos de locais em que você confia. Uma melhoria possível é avisar antes de abrir.
4. **Sem assinatura digital do .exe.** O Windows SmartScreen avisa na primeira execução. Veja `docs/GUIA-PUBLICAR.md`.
5. **Bloqueio por inatividade é opcional e vem desligado.** Ative em Configurações > Segurança em computadores compartilhados.
6. **Atualizações de Qt/QXlsx** são manuais: acompanhe os avisos de segurança do Qt e atualize a versão no workflow.
7. **A extensão do arquivo não prova o conteúdo.** Um `.pdf` pode ser outro formato. O programa abre pelo aplicativo
   padrão do Windows, então mantenha o leitor de PDF e o Office atualizados.

## LGPD (dados de alunos)

Notas, frequência, observações e **ocorrências** (conduta, dificuldade de aprendizagem, contatos com a família) sobre alunos são
**dados pessoais**, e as ocorrências podem expor informações delicadas; se o aluno for menor de idade, valem os cuidados
do art. 14 da LGPD. Registre fatos objetivos, evite diagnósticos e dados de saúde, e lembre que o banco não é criptografado (risco 1). O **Assistente de IA** envia o texto digitado a um serviço externo: nunca digite nomes ou dados pessoais de alunos nele.
Boas práticas: use só o necessário, não envie planilhas e backups por aplicativos de mensagem,
apague os dados de turmas antigas conforme a política da escola, e confirme com a escola quem é o controlador dos
dados antes de integrar serviços externos (Classroom, nuvem).

## Para quem for continuar o projeto

- Regra: **texto do usuário nunca vira HTML, fórmula ou SQL.** Use `toHtmlEscaped()`, `Qt::PlainText`,
  `escreverTexto()` e parâmetros (`:nome`) nas consultas.
- Novas dependências: fixar por versão/hash e registrar a licença.
- Antes de cada versão: rodar `ctest` e `ProfOrganizer.exe --selftest relatorio.txt` (o CI já faz).
