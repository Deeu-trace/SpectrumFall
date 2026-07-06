$ErrorActionPreference = "Stop"

$mermaidDir = "e:\SpectrumFall\report_assets\mermaid"
$imageDir = "e:\SpectrumFall\report_assets\images"

$files = @(
    @{ Name = "class-diagram-overall"; File = "class-diagram-overall.mmd" },
    @{ Name = "class-diagram-visualizer"; File = "class-diagram-visualizer.mmd" },
    @{ Name = "class-diagram-persistence"; File = "class-diagram-persistence.mmd" }
)

# Kroki API endpoint
$apiUrl = "https://kroki.io/mermaid/png/"

foreach ($entry in $files) {
    $mmdPath = Join-Path $mermaidDir $entry.File
    $pngPath = Join-Path $imageDir "$($entry.Name).png"

    Write-Host "Rendering $($entry.Name)..." -ForegroundColor Cyan

    if (-not (Test-Path $mmdPath)) {
        Write-Host "  [ERROR] Source file not found: $mmdPath" -ForegroundColor Red
        continue
    }

    $diagramSource = Get-Content -Path $mmdPath -Raw -Encoding UTF8

    # Kroki expects the diagram source as the request body, with appropriate headers
    $headers = @{ "Content-Type" = "text/plain" }

    try {
        $response = Invoke-WebRequest -Uri $apiUrl -Method Post -Headers $headers -Body $diagramSource -ContentType "text/plain" -TimeoutSec 60 -UseBasicParsing

        if ($response.StatusCode -eq 200) {
            [System.IO.File]::WriteAllBytes($pngPath, $response.Content)
            $size = (Get-Item $pngPath).Length
            Write-Host "  [OK] Saved to $pngPath ($size bytes)" -ForegroundColor Green
        } else {
            Write-Host "  [ERROR] HTTP $($response.StatusCode)" -ForegroundColor Red
        }
    } catch {
        Write-Host "  [ERROR] $($_.Exception.Message)" -ForegroundColor Red
    }
}

Write-Host "`nDone. Files in $imageDir :" -ForegroundColor Yellow
Get-ChildItem -Path $imageDir -Filter "*.png" | Format-Table Name, Length
