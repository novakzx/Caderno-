; Instalador do Caderno+ (Inno Setup 6). Gerado pelo workflow do GitHub Actions:
;   iscc /DVersao=1.1.0 installer\Caderno.iss
; Lê o pacote pronto em dist\ProfOrganizer (exe + DLLs do Qt) e grava dist\Caderno-Setup-<versao>.exe.
;
; Instala só para o usuário atual (sem pedir administrador), em %LOCALAPPDATA%\Programs\Caderno+.
; Os DADOS (turmas, notas, contas) ficam em %APPDATA%\ProfOrganizer e NUNCA são apagados ao desinstalar.

#ifndef Versao
  #define Versao "1.1.0"
#endif

[Setup]
; Identificador único do programa: NÃO mude (é ele que faz uma versão nova atualizar a anterior).
AppId={{6F1B7C52-3A4E-4D0B-9C61-2E7A5B8D4C13}
AppName=Caderno+
AppVersion={#Versao}
AppVerName=Caderno+ {#Versao}
AppPublisher=Caderno+
AppPublisherURL=https://github.com/novakzx/Caderno-
AppSupportURL=https://github.com/novakzx/Caderno-/issues
DefaultDirName={autopf}\Caderno+
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
; x64compatible (Inno Setup 6.3+) inclui o Windows 11 em ARM, que roda o programa por emulação.
#if Ver >= EncodeVer(6, 3, 0)
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#else
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
#endif
OutputDir=..\dist
OutputBaseFilename=Caderno-Setup-{#Versao}
SetupIconFile=..\resources\icons\app.ico
UninstallDisplayName=Caderno+
UninstallDisplayIcon={app}\ProfOrganizer.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; Se o Caderno+ estiver aberto durante uma atualização, o instalador pede para fechá-lo.
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Tasks]
Name: "desktopicon"; Description: "Criar um atalho na área de trabalho"; Flags: unchecked

[Files]
; autoteste.txt é só do CI; não vai para o computador do professor.
Source: "..\dist\ProfOrganizer\*"; DestDir: "{app}"; Excludes: "autoteste.txt"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{autoprograms}\Caderno+"; Filename: "{app}\ProfOrganizer.exe"
Name: "{autodesktop}\Caderno+"; Filename: "{app}\ProfOrganizer.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\ProfOrganizer.exe"; Description: "Abrir o Caderno+"; Flags: postinstall nowait skipifsilent

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    SuppressibleMsgBox('O Caderno+ foi removido do computador.' + #13#10 + #13#10 +
      'Seus dados (turmas, notas, frequência e contas) foram mantidos em %APPDATA%\ProfOrganizer, ' +
      'para você não perder nada ao reinstalar. Para apagá-los de vez, exclua essa pasta.',
      mbInformation, MB_OK, IDOK);
end;
