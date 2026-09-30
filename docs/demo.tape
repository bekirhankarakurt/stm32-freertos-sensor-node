# Demo recording: bootloader + application booted in QEMU, telemetry decoded live.
# Regenerate (after building the `host` and `firmware` presets): vhs docs/demo.tape
Output docs/demo.gif

Set Shell "bash"
Set FontSize 12
Set Width 1180
Set Height 430
Set Padding 16
Set Framerate 6
Set PlaybackSpeed 1.0
Set TypingSpeed 35ms
Set Theme "Dracula"

Hide
Type "export PS1='$ ' && clear"
Enter
Show

Type "# STM32F401 sensor node: bootloader + FreeRTOS app in QEMU, UART decoded by sn_decode"
Enter
Sleep 500ms
Type "qemu-system-arm -M netduinoplus2 -display none -monitor none -serial null -serial stdio \"
Enter
Type "    -kernel build/firmware/firmware/sn_full_qemu.bin | build/host/tools/sn_decode --port -"
Enter
Sleep 6s
Ctrl+C
Sleep 1s
