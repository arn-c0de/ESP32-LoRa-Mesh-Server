# Contributing to ESP32 LoRa Mesh Server

We welcome contributions that improve this project. Please read these guidelines before submitting.

## Getting Started

1. Fork the repository on GitHub
2. Clone your fork locally:
   ```bash
   git clone https://github.com/<your-username>/ESP32-LoRa-Mesh-Server.git
   cd ESP32-LoRa-Mesh-Server
   ```
3. Create a feature branch:
   ```bash
   git checkout -b feature/your-feature-name
   ```

## Development Setup

- Install `arduino-cli` (see README.md)
- Install all required libraries automatically via `./build.sh`
- Compile: `arduino-cli compile -b esp32:esp32:esp32 .`
- Flash: `./build.sh`

## Code Style

- **Headers (`.h`)**: Document all functions with comments
- **Main sketch (`.ino`)**: Keep modular; use header files for logic
- **Naming**: camelCase for variables, UPPERCASE for constants
- **Comments**: Clear, English, technical
- **No external formatting tools required**

## Commit Guidelines

- Use clear, descriptive commit messages
- Reference issues when applicable: `Fixes #123`
- Keep commits atomic (one logical change per commit)
- Example: `Add encryption key persistence to NVS`

## Testing

- Test on actual ESP32 LoRa32 hardware before submitting
- Verify serial commands work correctly
- Test both with and without encryption enabled
- Check OLED display updates properly
- Ensure mesh forwarding works (multi-hop test)

## Submitting Changes

### Bug Reports

Use GitHub Issues with:
- Clear title describing the problem
- Reproduction steps
- Expected vs. actual behavior
- Hardware setup (board, antenna, frequency)
- Serial output or error messages

### Feature Requests

Use GitHub Issues with:
- Clear title and description
- Motivation and use case
- Proposed implementation (if you have ideas)

### Pull Requests

1. Update documentation (README.md, HARDWARE.md) if needed
2. Add a clear PR title and description
3. Reference related issues
4. Ensure code compiles without warnings
5. Test on actual hardware
6. Keep PR scope focused (one feature per PR)

## Documentation

- Update README.md for user-facing changes
- Update HARDWARE.md for pin/hardware changes
- Add code comments for complex logic
- Include command examples in README if adding serial commands

## Project Structure

```
.
├── ESP32-LoRa-Mesh-Server.ino   Main sketch
├── mesh_network.h               Routing, PING/PONG, rebroadcasting
├── commands.h                   Serial command parser
├── crypt.h                      AES-256-GCM encryption
├── display_ui.h                 OLED display functions
├── build.sh                     Build and flash script
└── serial_monitor.sh            Serial monitor helper
```

## Areas for Contribution

- Bug fixes and stability improvements
- Command enhancements
- Documentation improvements
- Hardware compatibility testing
- Performance optimizations
- Security reviews (see SECURITY.md)

## Questions?

- Open an issue on GitHub
- Contact: arn-c0de@protonmail.com

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

---

Thank you for helping improve ESP32 LoRa Mesh Server!
