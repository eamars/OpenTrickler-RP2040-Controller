REM https://github.com/usedbytes/serial-flash
REM install go: https://go.dev/doc/install
REM run: "go install github.com/usedbytes/serial-flash@latest" to install serial-flash
REM serial-flash tcp:192.168.4.1:4242 build/app.elf
serial-flash tcp:192.168.2.163:4242 build/app.elf
pause