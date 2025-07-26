# 管理员powershell执行
$cert = New-SelfSignedCertificate -Type CodeSigning -Subject "CN=Debug Certificate" -KeyUsage DigitalSignature -KeyAlgorithm RSA -KeyLength 2048 -NotAfter (Get-Date).AddYears(1)
Export-PfxCertificate -Cert $cert -FilePath debug_cert.pfx -Password (ConvertTo-SecureString -String "debug_sign_passwd" -Force -AsPlainText)