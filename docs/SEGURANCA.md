# Segurança do Caderno+

Revisão do código feita em outubro de 2026. Ela cobre o programa em si (C++/Qt/SQLite), as contas locais
(login), a importação/exportação de arquivos e a entrega (GitHub Actions). Não substitui uma auditoria externa.

## O que o programa guarda e onde

| Dado | Onde | Observação |
| --- | --- | --- |
| Turmas, alunos, notas, frequência, anotações | `%APPDATA%\ProfOrganizer\ProfOrganizer\professor.db` (1ª conta) e `contas\<id>\professor.db` (demais) | SQLite, **sem criptografia** |
| Contas (nome, e-mail, hash da senha) | `...\contas.db` | só hash PBKDF2 e sal, nunca a senha |
| Backups | pasta `backups` ao lado do banco de cada conta | cópia do banco, também sem criptografia |
| Preferências | registro do Windows (QSettings) | tema, última seção, último e-mail usado |

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
5. **Sem trava por inatividade.** A janela fica aberta enquanto o programa estiver aberto. Dá para acrescentar
   "bloquear após N minutos" voltando à tela de login.
6. **Atualizações de Qt/QXlsx** são manuais: acompanhe os avisos de segurança do Qt e atualize a versão no workflow.
7. **A extensão do arquivo não prova o conteúdo.** Um `.pdf` pode ser outro formato. O programa abre pelo aplicativo
   padrão do Windows, então mantenha o leitor de PDF e o Office atualizados.

## LGPD (dados de alunos)

Notas, frequência e observações sobre alunos são **dados pessoais**; se o aluno for menor de idade, valem os cuidados
do art. 14 da LGPD. Boas práticas: use só o necessário, não envie planilhas e backups por aplicativos de mensagem,
apague os dados de turmas antigas conforme a política da escola, e confirme com a escola quem é o controlador dos
dados antes de integrar serviços externos (Classroom, nuvem).

## Para quem for continuar o projeto

- Regra: **texto do usuário nunca vira HTML, fórmula ou SQL.** Use `toHtmlEscaped()`, `Qt::PlainText`,
  `escreverTexto()` e parâmetros (`:nome`) nas consultas.
- Novas dependências: fixar por versão/hash e registrar a licença.
- Antes de cada versão: rodar `ctest` e `ProfOrganizer.exe --selftest relatorio.txt` (o CI já faz).
