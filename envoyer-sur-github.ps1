# Envoie le dossier smart-football sur https://github.com/Oussama1803/smart-football
# Usage (PowerShell, depuis Documents\smart-football) :  .\envoyer-sur-github.ps1
$ErrorActionPreference = "Stop"

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Host "Git n'est pas installe. Installe-le : winget install --id Git.Git -e" -ForegroundColor Red
    exit 1
}

Set-Location $PSScriptRoot

$remote = "https://github.com/Oussama1803/smart-football.git"
if (-not (git remote | Select-String -SimpleMatch "origin")) {
    git remote add origin $remote
}

git branch -M main
git pull origin main --allow-unrelated-histories --no-edit
git push -u origin main

Write-Host "Termine. Verifie sur $remote" -ForegroundColor Green
