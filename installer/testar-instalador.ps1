# Teste de ponta a ponta do instalador (roda no CI e na máquina de quem desenvolve), sem janela:
#   instala -> confere -> roda o autoteste do programa instalado -> atualiza por cima
#   -> recusa pacote corrompido -> recusa caminho malicioso (../) -> desinstala -> confere a limpeza.
#
# Uso: pwsh installer/testar-instalador.ps1 -Instalador release/Caderno-Setup-1.1.0.exe -Stub build-setup/Release/CadernoSetup.exe `
#          -PastaDoPrograma dist/ProfOrganizer -Trabalho $env:RUNNER_TEMP/teste-instalador
#
# Os atalhos de teste vão para uma pasta própria (--shortcuts-in): o Menu Iniciar e a área de trabalho de verdade não são tocados.
# Atenção: este teste NÃO usa --delete-data (isso apagaria os dados reais do usuário que roda o teste).
param(
    [Parameter(Mandatory = $true)] [string] $Instalador,
    [Parameter(Mandatory = $true)] [string] $Stub,
    [Parameter(Mandatory = $true)] [string] $PastaDoPrograma,
    [Parameter(Mandatory = $true)] [string] $Trabalho
)

$ErrorActionPreference = "Stop"
$falhas = 0
function Verificar([string] $nome, [bool] $condicao, [string] $detalhe = "") {
    if ($condicao) { Write-Host "  OK      $nome" }
    else { Write-Host "  FALHOU  $nome $detalhe"; $script:falhas++ }
}
function Rodar([string] $exe, [string[]] $argumentos, [int] $limiteSegundos = 180) {
    $p = Start-Process -FilePath $exe -ArgumentList $argumentos -PassThru -WindowStyle Hidden
    if (-not $p.WaitForExit($limiteSegundos * 1000)) { $p.Kill(); return -999 }
    return $p.ExitCode
}

$Instalador = (Resolve-Path $Instalador).Path
$Stub = (Resolve-Path $Stub).Path
$PastaDoPrograma = (Resolve-Path $PastaDoPrograma).Path
New-Item -ItemType Directory -Force $Trabalho | Out-Null
$Trabalho = (Resolve-Path $Trabalho).Path
$programa = Join-Path $Trabalho "programa"
$atalhos = Join-Path $Trabalho "atalhos"
$chaveRegistro = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\CadernoPlus"

Write-Host "== Instalador (silencioso) =="
$codigo = Rodar $Instalador @("--silent", "--dir", $programa, "--shortcuts-in", $atalhos)
Verificar "instalar: código de saída 0" ($codigo -eq 0) "(código $codigo)"
Verificar "ProfOrganizer.exe instalado" (Test-Path (Join-Path $programa "ProfOrganizer.exe"))
Verificar "Desinstalar.exe e a lista de arquivos criados" ((Test-Path (Join-Path $programa "Desinstalar.exe")) -and (Test-Path (Join-Path $programa "desinstalar.lst")))
Verificar "autoteste.txt do CI não foi instalado" (-not (Test-Path (Join-Path $programa "autoteste.txt")))
$esperados = (Get-ChildItem $PastaDoPrograma -Recurse -File | Where-Object { $_.Name -ne "autoteste.txt" }).Count
$instalados = (Get-ChildItem $programa -Recurse -File | Where-Object { $_.Name -notin @("Desinstalar.exe", "desinstalar.lst") }).Count
Verificar "todos os arquivos do pacote foram instalados ($instalados de $esperados)" ($instalados -eq $esperados)
$hashOriginal = (Get-FileHash (Join-Path $PastaDoPrograma "ProfOrganizer.exe") -Algorithm SHA256).Hash
$hashInstalado = (Get-FileHash (Join-Path $programa "ProfOrganizer.exe") -Algorithm SHA256).Hash
Verificar "o ProfOrganizer.exe instalado é idêntico ao original (SHA-256)" ($hashOriginal -eq $hashInstalado)
Verificar "atalhos criados" ((Test-Path (Join-Path $atalhos "Menu Iniciar - Caderno+.lnk")) -and (Test-Path (Join-Path $atalhos "Area de trabalho - Caderno+.lnk")))
Verificar "registro do Windows (Configurações > Aplicativos)" ((Test-Path $chaveRegistro) -and ((Get-ItemProperty $chaveRegistro).InstallLocation -eq $programa))

Write-Host "== Programa instalado =="
$relatorio = Join-Path $Trabalho "autoteste-instalado.txt"
$codigo = Rodar (Join-Path $programa "ProfOrganizer.exe") @("--selftest", $relatorio)
Verificar "autoteste do programa instalado passa" ($codigo -eq 0) "(código $codigo)"

Write-Host "== Atualização por cima =="
Set-Content (Join-Path $programa "arquivo-do-usuario.txt") "isto não é do Caderno+"
[System.IO.File]::WriteAllText((Join-Path $programa "Qt6Core.dll"), "arquivo estragado")   # simula um arquivo antigo/diferente
$codigo = Rodar $Instalador @("--silent", "--dir", $programa, "--shortcuts-in", $atalhos)
Verificar "reinstalar por cima: código 0" ($codigo -eq 0) "(código $codigo)"
Verificar "arquivo estragado foi substituído pelo original" ((Get-FileHash (Join-Path $programa "Qt6Core.dll") -Algorithm SHA256).Hash -eq (Get-FileHash (Join-Path $PastaDoPrograma "Qt6Core.dll") -Algorithm SHA256).Hash)
Verificar "arquivo que não é do Caderno+ foi preservado" (Test-Path (Join-Path $programa "arquivo-do-usuario.txt"))

Write-Host "== Pacote corrompido =="
$corrompido = Join-Path $Trabalho "corrompido.exe"
$bytes = [System.IO.File]::ReadAllBytes($Instalador)
$meio = [int]($bytes.Length * 0.6)
$bytes[$meio] = $bytes[$meio] -bxor 0xFF
[System.IO.File]::WriteAllBytes($corrompido, $bytes)
$destinoCorrompido = Join-Path $Trabalho "destino-corrompido"
$codigo = Rodar $corrompido @("--silent", "--dir", $destinoCorrompido, "--shortcuts-in", (Join-Path $Trabalho "atalhos-corrompido"))
Verificar "instalador adulterado é recusado (código diferente de 0)" ($codigo -ne 0) "(código $codigo)"

Write-Host "== Caminho malicioso no pacote (../) =="
$malicioso = Join-Path $Trabalho "malicioso.exe"
python (Join-Path $PSScriptRoot "empacotar.py") --stub $Stub --pasta $PastaDoPrograma --versao 0.0.0 --saida $malicioso --inseguro-para-teste | Out-Null
$destinoMalicioso = Join-Path (Join-Path $Trabalho "destino-malicioso") "interno"
$codigo = Rodar $malicioso @("--silent", "--dir", $destinoMalicioso, "--shortcuts-in", (Join-Path $Trabalho "atalhos-malicioso"))
Verificar "pacote com ../ é recusado" ($codigo -ne 0) "(código $codigo)"
Verificar "nada foi gravado fora da pasta de destino" (-not (Test-Path (Join-Path $Trabalho "destino-malicioso\fora-da-pasta.txt")))
Verificar "o pacote malicioso não instalou nada (os caminhos são conferidos antes)" (-not (Test-Path (Join-Path $destinoMalicioso "ProfOrganizer.exe")))

Write-Host "== Desinstalador (silencioso) =="
$codigo = Rodar (Join-Path $programa "Desinstalar.exe") @("--silent", "--shortcuts-in", $atalhos)
Verificar "desinstalar: código de saída 0" ($codigo -eq 0) "(código $codigo)"
for ($i = 0; $i -lt 20 -and (Test-Path (Join-Path $programa "Desinstalar.exe")); $i++) { Start-Sleep -Milliseconds 500 }   # o desinstalador se apaga logo depois de sair
Verificar "programa removido" (-not (Test-Path (Join-Path $programa "ProfOrganizer.exe")))
Verificar "desinstalador e lista removidos" ((-not (Test-Path (Join-Path $programa "Desinstalar.exe"))) -and (-not (Test-Path (Join-Path $programa "desinstalar.lst"))))
Verificar "arquivo do usuário continua lá (a pasta não foi apagada às cegas)" (Test-Path (Join-Path $programa "arquivo-do-usuario.txt"))
Verificar "subpastas do Qt removidas" (-not (Test-Path (Join-Path $programa "platforms")))
Verificar "atalhos removidos" ((-not (Test-Path (Join-Path $atalhos "Menu Iniciar - Caderno+.lnk"))) -and (-not (Test-Path (Join-Path $atalhos "Area de trabalho - Caderno+.lnk"))))
Verificar "registro limpo" (-not (Test-Path $chaveRegistro))

Write-Host ""
if ($falhas -eq 0) { Write-Host "RESULTADO: tudo certo."; exit 0 }
Write-Host "RESULTADO: $falhas verificação(ões) falharam."
exit 1
