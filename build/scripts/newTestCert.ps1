# CreateTestCertificate.ps1
$cert = New-SelfSignedCertificate `
    -Subject "CN=gh503 Test, O=angus_robot@163.com" `
    -FriendlyName "gh503 Certificate ONLY for Test" `
    -Provider "" `
    -Type CodeSigning `
    -KeyUsage DigitalSignature `
    -KeyAlgorithm RSA `
    -KeyLength 2048 `
    -CertStoreLocation "Cert:\CurrentUser\My"

$password = ConvertTo-SecureString -String "password" -Force -AsPlainText

Export-PfxCertificate `
    -Cert $cert `
    -FilePath "certs\gh503_test_certificate.pfx" `
    -Password $password

Write-Host "Test certificate created: gh503_test_certificate.pfx"
Write-Host "Certification Installed Succ: open certlm.msc: Certificates - CurrentUser\My\"