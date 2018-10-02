# yes this is dumb and should be done with autoconf or something
cat << EOF | gcc -xc - 2>&1
#include <windows.h>
#include <xinput.h>
XINPUT_GAMEPAD_EX x1;
int main(int argc, char **argv) { }
EOF
