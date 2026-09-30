# Security Policy

## Supported versions

| Version | Supported |
|---|---|
| 1.0.x | Yes |
| < 1.0 | No |

## Reporting a vulnerability

Please report vulnerabilities privately through GitHub's
[security advisory form](https://github.com/bekirhankarakurt/stm32-freertos-sensor-node/security/advisories/new)
instead of opening a public issue. Include the affected version, a description of the impact and steps to reproduce.
You can expect an acknowledgement within 7 days and a fix or mitigation plan within 30 days.

## Scope and known limitations

- Firmware images are protected against corruption (CRC-32), **not** against tampering: there is no cryptographic
  signature yet, so anyone with access to the UART can install firmware. Signed images are on the roadmap.
- The update protocol has no authentication or encryption; deploy the node only where the serial link is trusted.
- Read-out protection (RDP) and write protection of the bootloader sector are not configured by the build.
