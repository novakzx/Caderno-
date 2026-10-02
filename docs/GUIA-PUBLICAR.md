# Guia: como colocar o Caderno+ "no ar"

O Caderno+ é um **programa de computador (Windows)** que roda offline. Ele não "vai para o ar" como um site: o que se
publica é o **instalador/pacote**, para as pessoas baixarem. Se a ideia for ter o sistema **numa página da internet, que
abre em qualquer aparelho**, isso é outro produto (veja o item 6).

## 1. Distribuir o programa (o caminho mais simples)

### 1.1 Lançar uma versão (já automatizado)

Todo `push` gera, além da pasta do programa, o **instalador** (`Caderno-Setup-<versão>.exe`), um `.zip` portátil e o
`SHA256SUMS.txt` (artefato **Instalador-build-N-xxxxxxx**, na página da execução em Actions). Quando o push é uma
**etiqueta `vX.Y.Z`**, o workflow também publica uma **Release** no GitHub com esses três arquivos, com link público e permanente.

Para lançar a versão 1.2.0, por exemplo:

1. Aumente `VERSION` em `CMakeLists.txt` (`project(ProfOrganizer VERSION 1.2.0 ...)`). É a **única fonte** do número: ele vai para
   a barra de título, a tela de login e o instalador.
2. Faça commit e envie. Espere o build ficar verde (compila, roda `ctest` e o autoteste).
3. Crie a etiqueta **igual à versão** e envie:
   ```bash
   git tag v1.2.0
   git push origin v1.2.0
   ```
4. Em alguns minutos aparece em **Releases** (lateral direita da página do repositório). Os textos da Release são gerados a partir dos commits;
   você pode editá-los depois no GitHub.

O workflow **recusa** a etiqueta se o formato não for `vX.Y.Z` ou se o número não bater com o `CMakeLists.txt` (para não publicar um
instalador com a versão errada). A permissão de escrita (`contents: write`) existe só no job que cria a Release.

> Esta parte foi escrita e revisada, mas a primeira etiqueta real é o teste final: depois de criá-la, confira em Actions que o job
> "Publicar a Release" ficou verde e baixe o instalador **num computador limpo** antes de divulgar.

### 1.2 O instalador (Inno Setup)

O script é `installer/Caderno.iss`. O instalador:

- instala **só para o usuário atual** (sem pedir administrador), em `%LOCALAPPDATA%\Programs\Caderno+`;
- cria o atalho no Menu Iniciar (e, se a pessoa marcar, na área de trabalho) e o desinstalador;
- **atualiza por cima** de uma versão anterior (o `AppId` é fixo: não o altere) e pede para fechar o Caderno+ se estiver aberto;
- **não apaga os dados** (`%APPDATA%\ProfOrganizer`) ao desinstalar; avisa onde eles ficam.

Para gerar localmente (com o Inno Setup 6 instalado e a pasta `dist\ProfOrganizer` pronta): `iscc /DVersao=1.2.0 installer\Caderno.iss`.

### 1.3 O aviso azul do Windows (SmartScreen)

Programas sem assinatura mostram "O Windows protegeu o seu computador". Opções:

- **Assinatura de código**: certificado pago (OV/EV) ou o serviço **Azure Trusted Signing**. É a solução definitiva.
- **Microsoft Store (MSIX)**: conta de desenvolvedor individual com taxa única; a loja assina o pacote.
- **Reputação**: o aviso diminui com o tempo e o número de downloads; até lá, oriente: "Mais informações → Executar assim mesmo".
- O **hash SHA-256** de cada arquivo já vai na Release (`SHA256SUMS.txt`); quem quiser conferir roda `Get-FileHash Caderno-Setup-1.2.0.exe`
  e compara com o valor do arquivo.

## 2. Uma página para divulgar

Uma página simples (nome, o que faz, capturas de tela, botão "Baixar", como instalar, política de privacidade) basta.
Hospedagem gratuita: **GitHub Pages**, **Vercel**, **Netlify** ou **Cloudflare Pages**. O botão aponta para o
`.../releases/latest`. Posso criar essa página com o visual do Caderno+ se quiser.

## 3. Atualizações

- Mais simples: o programa consulta `https://api.github.com/repos/<usuario>/Caderno-/releases/latest` ao abrir, compara a
  versão e mostra "Há uma versão nova. Baixar" (sem baixar nem executar nada sozinho).
- Atualização automática de verdade: **Qt Installer Framework** ou **WinSparkle**. Mais trabalho; só vale com muitos usuários.
- Antes de lançar uma versão que muda o banco: a migração roda sozinha, mas **o programa já faz backup ao abrir/fechar**.
  Teste com uma cópia de um banco antigo.

## 4. Antes de divulgar

- **Licença do seu código**: o repositório é público e, sem arquivo `LICENSE`, ninguém tem permissão de uso além de ver.
  Escolha uma (MIT é a mais simples) ou deixe como "todos os direitos reservados".
- **Licenças de terceiros** (devem acompanhar o programa):
  - **Qt** — LGPLv3: use as DLLs dinâmicas (já é assim), inclua o aviso/licença e diga onde obter o código-fonte do Qt.
  - **QXlsx** — MIT. **Figtree** — SIL OFL (o `OFL.txt` já vai no pacote).
  - **Nome e logotipo "Caderno+"**: verifique se não há marca registrada igual (consulta no INPI).
- **Política de privacidade**: o programa é offline e não coleta dados; diga isso, e explique onde ficam os arquivos.
- **LGPD**: dados de alunos são pessoais; avise que a responsabilidade pelos dados é de quem usa (a escola/o professor).
- **Teste em computador limpo** (outro usuário do Windows ou máquina virtual): sem Qt instalado, sem a sua pasta de dados.

## 5. Checklist de lançamento

1. `ctest` e `--selftest` verdes (o CI já roda).
2. Versão aumentada; texto do que mudou escrito.
3. Etiqueta `vX.Y.Z` criada e Release publicada com `.zip` (e instalador).
4. SHA-256 publicado.
5. Página/anúncio atualizados.
6. Testado o download e a instalação num computador limpo.

## 6. E se eu quiser um sistema online (abre no navegador, em qualquer aparelho)?

É um **projeto novo**, não um ajuste:

- Precisa de **servidor** (back-end + banco de dados na nuvem), **contas de verdade** (e-mail, recuperação, 2 fatores) e
  **hospedagem** com custo mensal.
- Os dados dos alunos passam a ficar num servidor: exige **LGPD completa** (base legal, termo, política, contrato com a
  escola, segurança, backup, exclusão a pedido).
- O visual e as regras (média, frequência, horário) podem ser reaproveitados, mas a interface seria reescrita em
  tecnologia web. O Qt tem versão para navegador (WebAssembly), mas SQLite, arquivos e PDF funcionam de forma limitada.
- Caminho intermediário: manter o programa offline e acrescentar **sincronização opcional** entre aparelhos.

Se esse for o objetivo, o melhor primeiro passo é definir quem usa (só você? a escola?), em quais aparelhos e quanto
tempo você pode manter um serviço no ar.
