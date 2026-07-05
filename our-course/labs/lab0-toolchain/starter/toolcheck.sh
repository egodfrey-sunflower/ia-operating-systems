#!/usr/bin/env bash
#
# toolcheck.sh -- verify the Lab 0 toolchain is installed.
#
# Prints an OK/MISSING table for each required tool and exits nonzero if any
# are missing. This script only checks for presence on $PATH; it does not run
# QEMU or compile anything.

set -u

# Tool name -> short description. Order is preserved.
tools=(
	"gcc|host C compiler"
	"make|build tool"
	"gdb-multiarch|cross-target debugger (RISC-V)"
	"qemu-system-riscv64|RISC-V system emulator"
	"riscv64-unknown-elf-gcc|RISC-V cross compiler"
	"strace|syscall tracer (Part D)"
)

missing=0

printf '%-26s %-8s %s\n' "TOOL" "STATUS" "DESCRIPTION"
printf '%-26s %-8s %s\n' "----" "------" "-----------"

for entry in "${tools[@]}"; do
	name=${entry%%|*}
	desc=${entry#*|}
	if command -v "$name" >/dev/null 2>&1; then
		status="OK"
	else
		status="MISSING"
		missing=$((missing + 1))
	fi
	printf '%-26s %-8s %s\n' "$name" "$status" "$desc"
done

echo
if [ "$missing" -ne 0 ]; then
	echo "$missing tool(s) missing."
	echo "On Ubuntu 24.04, install everything with:"
	echo
	echo "    sudo apt update"
	echo "    sudo apt install build-essential gdb-multiarch \\"
	echo "        gcc-riscv64-unknown-elf qemu-system-misc strace"
	echo
	echo "(build-essential provides gcc and make; qemu-system-misc provides"
	echo " qemu-system-riscv64; gcc-riscv64-unknown-elf provides the cross"
	echo " compiler as riscv64-unknown-elf-gcc; strace is used in Part D.)"
	exit 1
fi

echo "All required tools present."
exit 0
