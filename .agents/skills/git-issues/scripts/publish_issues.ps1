param(
    [Parameter(Mandatory=$true)]
    [string]$Repo,

    [Parameter(Mandatory=$true)]
    [string]$Title,

    [Parameter(Mandatory=$true)]
    [string]$Body,

    [Parameter(Mandatory=$false)]
    [string]$Label = "enhancement"
)

$ghPath = "gh"
if (-not (Get-Command "gh" -ErrorAction SilentlyContinue)) {
    $defaultGh = "C:\Program Files\GitHub CLI\gh.exe"
    if (Test-Path $defaultGh) {
        $ghPath = $defaultGh
    } else {
        Write-Error "GitHub CLI (gh) no encontrado. Ejecutá: winget install --id GitHub.cli"
        exit 1
    }
}

& $ghPath issue create -R $Repo --title $Title --body $Body --label $Label
