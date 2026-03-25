<!-- See COPYING.txt for license details. -->

# M1 Development Guidelines

## Coding Standards

- Follow STM32 HAL coding conventions
- Use meaningful variable and function names
- Prefer camelCase for variables/functions, UPPER_CASE for constants
- Use appropriate integer types (`uint8_t`, `uint16_t`, etc.)
- Add comments for non-obvious logic

## Branch Naming and Commit Messages

See [`.github/GUIDELINES.md`](.github/GUIDELINES.md) for branch naming conventions and
Conventional Commits format used in this repository.

## Testing

- Test on hardware when possible
- Document test scenarios and edge cases
- Ensure NFC, RFID, and Sub-GHz functionality are verified
