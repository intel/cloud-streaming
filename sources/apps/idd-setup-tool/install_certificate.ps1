<#
.SYNOPSIS
Install certificate into certificate store

.DESCRIPTION
Given the path to a certificate (.cat file) this installs the sertificate as a trusted in all certificate stores on the system.

.PARAMETER driverFile
Path to the certificate (.cat file) to be installed.

.OUTPUTS
No outputs.

#>

#Requires -RunAsAdministrator

param (
    [string]$driverFile = ''
)

$tempFile = New-TemporaryFile;
$certStores = "TrustedPublisher";
$exportType = [System.Security.Cryptography.X509Certificates.X509ContentType]::Cert;

write-Output "Trusting certificate for: $driverFile"

$cert = (Get-AuthenticodeSignature $driverFile).SignerCertificate;
if ($NULL -eq $cert) {
    $msg = "  Warning: {0} is not signed, cannot extract certificate" -f $driverFile
    Write-Output $msg
    exit 0
}
[System.IO.File]::WriteAllBytes($tempFile, $cert.Export($exportType));

foreach ($store in $certStores)
{
    certutil -addstore $store $tempFile
}
