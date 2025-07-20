Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -match "gh503 Test" } | Remove-Item
Write-Host "Delete Succ."