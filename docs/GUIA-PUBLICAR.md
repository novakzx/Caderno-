# Guia: como colocar o Caderno+ "no ar"

O Caderno+ é um **programa de computador (Windows)** que roda offline. Ele não "vai para o ar" como um site: o que se
publica é o **instalador/pacote**, para as pessoas baixarem. Se a ideia for ter o sistema **numa página da internet, que
abre em qualquer aparelho**, isso é outro produto (veja o item 6).

## 1. Distribuir o programa (o caminho mais simples)

### 1.1 GitHub Releases (grátis)

Hoje cada `push` gera um pacote como "artefato" (some em 90 dias e exige login no GitHub para baixar). O próximo passo
é publicar uma **Release**, com link público e permanente:

1. Aumente a versão em `CMakeLists.txt` (`project(... VERSION 1.1.0)`) e em `src/main.cpp` (`setApplicationVersion`).
2. Crie uma etiqueta e envie:
   ```bash
   git tag v1.1.0
   git push origin v1.1.0
   ```
3. Em **Releases → Draft a new release**, escolha a etiqueta, escreva o que mudou e anexe o `.zip` do pacote.

Dá para automatizar: um job que roda quando aparece uma etiqueta `v*`, compacta `dist/ProfOrganizer` e cria a Release com
`gh release create` (permissão `contents: write` só nesse job). Posso configurar isso quando você quiser.

### 1.2 Instalador de verdade (recomendado para professores)

Um `.zip` pede para extrair e achar o `.exe`. Um instalador cria atalho no Menu Iniciar, tem desinstalador e instala
sem precisar de administrador. A ferramenta gratuita é o **Inno Setup**. Modelo mínimo (`installer/Caderno.iss`):

```ini
[Setup]
AppName=Caderno+
AppVersion=1.1.0
DefaultDirName={autopf}\Caderno+
PrivilegesRequired=lowest
OutputBaseFilename=Caderno-Setup-1.1.0
Compression=lzma2
SolidCompression=yes
SetupIconFile=..\resources\icons\app.ico

[Files]
Source: "..\dist\ProfOrganizer\*"; DestDir: "{app}"; Flags: recursesubdirs

[Icons]
Name: "{autoprograms}\Caderno+"; Filename: "{app}\ProfOrganizer.exe"
Name: "{autodesktop}\Caderno+"; Filename: "{app}\ProfOrganizer.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Criar atalho na área de trabalho"

[Run]
Filename: "{app}\ProfOrganizer.exe"; Description: "Abrir o Caderno+"; Flags: postinstall nowait
```

O instalador **não** apaga os dados do usuário ao desinstalar (ficam em `%APPDATA%\ProfOrganizer`), o que é o desejado.
Nos executores do GitHub o Inno Setup já vem instalado (`iscc installer\Caderno.iss`). Este modelo não foi testado neste
projeto; teste num computador limpo antes de distribuir.

### 1.3 O aviso azul do Windows (SmartScreen)

Programas sem assinatura mostram "O Windows protegeu o seu computador". Opções:

- **Assinatura de código**: certificado pago (OV/EV) ou o serviço **Azure Trusted Signing**. É a solução definitiva.
- **Microsoft Store (MSIX)**: conta de desenvolvedor individual com taxa única; a loja assina o pacote.
- **Reputação**: o aviso diminui com o tempo e o número de downloads; até lá, oriente: "Mais informações → Executar assim mesmo".
- Sempre publique o **hash SHA-256** do arquivo para quem quiser conferir (`Get-FileHash arquivo.zip`).

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
