# Contributing to AVR-Init

Thank you for your interest in improving this AVR template!

## How to Contribute

1. **Bug Reports:** If the Makefile fails on your system or the script has a typo, please open an Issue.
2. **Feature Requests:** Want support for other programmers (like Arduino as ISP)? Open an Issue to discuss.
3. **Pull Requests:** 
   - Fork the repository.
   - Create a feature branch (`git checkout -b feature/amazing-feature`).
   - Commit your changes.
   - Push to the branch and open a Pull Request.

## Modifying the Template

If you want to change the default code or Makefile:
1. Modify the `avr-init.sh` script.
2. Update the `cat <<EOF` sections which contain the boilerplate code for `main.c` and `.gitignore`.
3. Ensure you update the `TEMPLATE_DIR` variable if you move the master files.

## Code Style
- Keep the Makefile portable.
- Use non-blocking code in examples (prefer `millis()` over `_delay_ms()`).
