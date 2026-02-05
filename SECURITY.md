# Security Policy

## Reporting a Vulnerability

If you discover a security vulnerability in this project, please report it responsibly and confidentially.

**Do NOT open a public GitHub issue.** Public disclosure of unpatched vulnerabilities can put users at risk.

### Reporting Process

1. Send a report to: **arn-c0de@protonmail.com**
2. Include:
   - Description of the vulnerability
   - Affected component(s) or file(s)
   - Severity assessment (low, medium, high, critical)
   - Reproduction steps (if applicable)
   - Proof-of-concept code (optional, helps verification)

3. Wait for acknowledgment (expect response within 48 hours)
4. Work with maintainers on a fix timeline
5. Coordinated disclosure: fixes are released before public announcement

### What to Expect

- Acknowledgment of your report
- Assessment of the vulnerability
- Discussion of remediation approach
- Timeline for patch release
- Credit in security advisory (if desired)

## Security Considerations for Users

### Encryption

- AES-256-GCM provides authenticated encryption
- Passphrases are derived using SHA-256
- Keys are stored in NVS (Preferences) on the ESP32
- Always use strong passphrases
- Clear keys with `/CLEARKEY` when no longer needed

### Radio Security

- LoRa is not inherently secure; it is a broadcast medium
- Without encryption, all messages are transmitted in plaintext
- Use `/KEY` and `/ESEND` for sensitive communications
- Verify that receiving nodes use the same encryption key

### Hardware Security

- The ESP32 is a general-purpose microcontroller, not a security appliance
- Physical access to the device may allow memory dumping
- Consider deployment environment and threat model
- Use proper enclosures to deny physical access if needed

### Network Security

- This implementation does not authenticate nodes
- Any device on the same frequency can receive or relay messages
- Use encryption for confidentiality
- Consider additional application-layer authentication for critical systems

## Known Limitations

- No entity authentication: any node can impersonate another
- No integrity verification without encryption
- No protection against replay attacks
- Radio range and reliability vary with environment
- LoRa duty-cycle restrictions apply in many regions

## Dependency Security

This project uses the following key dependencies:

- **RadioLib** (v7.5.0) - SX1276 LoRa radio driver
- **Adafruit SSD1306** (v2.5.16) - OLED display driver
- **Adafruit GFX** (v1.12.4) - Graphics library
- **mbedTLS** (included in ESP32 SDK) - Cryptography

We periodically check for updates to these libraries. Users should keep their ESP32 Arduino core and libraries up to date.

To update libraries:
```bash
arduino-cli lib upgrade
```

## Security Best Practices for Deployment

1. Use strong, random passphrases for encryption
2. Change passphrases regularly if keys are compromised
3. Deploy in secure physical locations
4. Monitor logs for unexpected activity
5. Disable features you don't use
6. Test thoroughly before production deployment
7. Keep ESP32 board firmware updated
8. Use appropriate TX power for your region

## Legal Compliance

- Verify all radio regulations in your jurisdiction before deploying
- Respect duty-cycle limitations
- Use appropriate power levels
- Never transmit without a proper antenna
- Consider spectrum licensing if required in your region

See [README.md](README.md) and [HARDWARE.md](HARDWARE.md) for regional regulations.

## Responsible Disclosure Timeline

- Day 0: Vulnerability reported
- Day 1: Acknowledgment and initial assessment
- Day 7-14: Patch development and testing
- Day 21: Security advisory and fix published
- After: Public disclosure

Timelines may be adjusted based on severity and complexity.

## Security Advisories

Past and future security issues are documented in GitHub Security Advisories:
https://github.com/arn-c0de/ESP32-LoRa-Mesh-Server/security/advisories

---

Thank you for helping keep this project secure.
