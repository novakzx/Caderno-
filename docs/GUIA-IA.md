# Guia: Assistente de IA (Cloudflare Workers AI)

O Caderno+ tem um **Assistente de IA** que escreve planos de aula, questões de prova, atividades, comunicados aos pais e
adapta textos. Ele usa o **Cloudflare Workers AI**, que tem um uso **gratuito diário** (10.000 "neurons" por dia, que
zeram à meia-noite UTC, 21h em Brasília). O programa **não traz nenhuma chave embutida**: cada professor usa a **própria**
conta gratuita. Assim ninguém divide a cota de ninguém e a chave nunca fica no código.

> Sem internet o Assistente não funciona, mas o resto do Caderno+ continua normal. Nada é enviado até você clicar em **Gerar**.

## 1. Criar a conta e a chave (uma vez só, uns 5 minutos)

Isto é feito por você, no site do Cloudflare (o programa não cria conta nem acessa o site por você):

1. Crie uma conta gratuita em https://dash.cloudflare.com (cadastro com e-mail e senha; não precisa de cartão para o plano gratuito).
2. No painel, abra **Workers AI** (pode estar em "AI" no menu lateral) e escolha **Use REST API**.
3. Copie o **Account ID** (32 letras e números) que aparece ali.
4. Clique em **Create a Workers AI API Token** e confirme. O token precisa da permissão **Workers AI** (leitura e edição).
   Copie o token **na hora**: o Cloudflare só mostra ele uma vez.

O painel do Cloudflare muda de lugar de vez em quando; se não achar, procure por "Workers AI" na documentação:
https://developers.cloudflare.com/workers-ai/get-started/rest-api/

## 2. Colocar no Caderno+

1. Abra **Configurações > Assistente de IA** (barra lateral).
2. Cole o **Account ID** e o **Token de API**. Deixe o modelo padrão (`@cf/meta/llama-3.1-8b-instruct-fp8`, rápido e econômico).
3. Clique em **Testar conexão**. Se aparecer "Conexão funcionando", clique em **Salvar**.

A chave é guardada **protegida pelo Windows** (DPAPI): só o seu usuário, neste computador, consegue abri-la. Para apagar, use
**Apagar chave salva** na mesma tela.

## 3. Usar

Abra **Assistente** na barra lateral, escolha o que criar (plano de aula, questões, atividades, comunicado, adaptar texto ou
pergunta livre), preencha os campos e clique em **Gerar**. A resposta aparece aos poucos e você pode editá-la.
Depois: **Copiar**, **Salvar como anotação** (fica na tela Anotações com a tag "ia") ou **Criar plano de aula** (vai para a tela Aulas).

**Sempre revise o que a IA escreveu.** Ela pode errar datas, fatos e códigos da BNCC; o Caderno+ avisa isso em toda resposta salva.

## 4. Privacidade (leia)

- Só vai para o Cloudflare **o texto que você escreve** nos campos do Assistente (disciplina, série, tema, observações...). O
  Caderno+ **nunca anexa** dados do banco (alunos, notas, frequência, ocorrências).
- **Não digite nomes nem dados pessoais de alunos.** Escreva "um aluno do 6º ano com dificuldade de leitura", não o nome dele.
  Isto vale pela LGPD, ainda mais para menores de idade.
- Os pedidos passam pela internet por HTTPS e são processados pelo Cloudflare, sujeito aos termos e à política de privacidade dele.
  Se a sua escola proíbe serviços externos com dados de alunos, **não use** o Assistente (ele só recebe o que você digitar, então
  dá para usá-lo só com temas gerais de aula).

## 5. Problemas comuns

| Mensagem | O que fazer |
| --- | --- |
| "O token foi recusado" | Confira se copiou o token inteiro e se ele tem a permissão **Workers AI** (leitura e edição). |
| "Não encontrei esse modelo ou esse Account ID" | Copie o Account ID de novo; confira o modelo (formato `@cf/empresa/nome`). |
| "O limite gratuito de hoje acabou" | Espere a cota zerar (meia-noite UTC, 21h em Brasília). Modelos maiores gastam a cota mais rápido; o padrão rende bastante. |
| "Sem conexão com a internet" | Verifique a rede. O resto do programa funciona sem internet. |

## 6. Para quem for continuar o projeto

- Código: `services/IaService` (cliente HTTP com resposta em fluxo SSE), `services/IaPrompts` (monta os pedidos),
  `services/SegredoService` (DPAPI), `ui/AssistentePage`, `ui/ConfiguracoesDialog`.
- Regras de segurança do cliente: só HTTPS para `api.cloudflare.com`; token validado (sem quebra de linha) e só no cabeçalho
  `Authorization`; redirecionamentos não são seguidos; o texto da IA é sempre mostrado como **texto simples** (nunca HTML).
- Os testes do autoteste usam um servidor HTTP falso em `127.0.0.1` (`IaService::usarServidorDeTeste` só aceita esse endereço).
  **A chamada ao Cloudflare de verdade nunca foi testada neste projeto** (não há chave no repositório): teste com a sua conta
  antes de depender dela e, se a API mudar, ajuste `IaService::montarCorpo` / `trechoDaLinhaSse`.
