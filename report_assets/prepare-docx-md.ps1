$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

# Find source markdown by globbing (avoid Chinese literal in script)
$sourceMd = (Get-ChildItem -Path "e:\SpectrumFall" -Filter "*.md" | Where-Object { $_.Name -notmatch "README|TODO|CHANGELOG" -and $_.Length -gt 50000 } | Sort-Object Length -Descending | Select-Object -First 1).FullName

if (-not $sourceMd) {
    Write-Host "ERROR: source markdown not found" -ForegroundColor Red
    exit 1
}

Write-Host "Source: $sourceMd"

$targetMd = "e:\SpectrumFall\report_docx_source.md"

Write-Host "Reading source markdown..." -ForegroundColor Cyan
$content = Get-Content -Path $sourceMd -Raw -Encoding UTF8

# Build image references using string concatenation (avoid backtick escapes in double-quoted strings)
$crlf = [char]13 + [char]10
$images = @(
    ('![图 3-1 系统整体类图](report_assets/images/class-diagram-overall.png)' + $crlf + $crlf + '**图 3-1 系统整体类图**'),
    ('![图 3-2 可视化模块类图](report_assets/images/class-diagram-visualizer.png)' + $crlf + $crlf + '**图 3-2 可视化模块类图**'),
    ('![图 3-3 数据持久化类图](report_assets/images/class-diagram-persistence.png)' + $crlf + $crlf + '**图 3-3 数据持久化类图**')
)

# Use regex to find all mermaid blocks (non-greedy, multiline) and replace them in order
$pattern = '(?s)```mermaid.*?```'
$matches = [regex]::Matches($content, $pattern)
Write-Host ('Found ' + $matches.Count + ' mermaid blocks via regex') -ForegroundColor Cyan

if ($matches.Count -gt 0) {
    # Replace from end to start to preserve indices
    for ($i = $matches.Count - 1; $i -ge 0; $i--) {
        $m = $matches[$i]
        $imgIdx = [Math]::Min($i, $images.Count - 1)
        $replacement = $images[$imgIdx]
        $content = $content.Substring(0, $m.Index) + $replacement + $content.Substring($m.Index + $m.Length)
        Write-Host ('  [OK] Replaced block #' + ($i + 1)) -ForegroundColor Green
    }
}

# Write the target file
Write-Host ('Writing target markdown: ' + $targetMd) -ForegroundColor Cyan
[System.IO.File]::WriteAllText($targetMd, $content, [System.Text.UTF8Encoding]::new($true))

$size = (Get-Item $targetMd).Length
Write-Host ('Done. File size: ' + $size + ' bytes') -ForegroundColor Green
