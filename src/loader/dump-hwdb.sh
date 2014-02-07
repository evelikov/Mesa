#!/bin/sh

set -e

function modalias_list() {
    while read vendor driver; do
        pci_id_file=$1/${driver}_pci_ids.h
        if ! test -r $pci_id_file; then
            printf "$driver pci:v%08X*bc03*\n" $vendor
            continue
        fi

        gcc -E $pci_id_file |
        while IFS=' (,' read c id rest; do
            test -z "$id" && continue
            printf "$driver pci:v%08Xd%08X*\n" $vendor $id
        done
    done <<EOF
0x8086 i915
0x8086 i965
0x1002 radeon
0x1002 r200
0x1002 r300
0x1002 r600
0x1002 radeonsi
0x10de nouveau
0x15ad vmwgfx
EOF
}

case $2 in
    hwdb)
        modalias_list $1 | while read driver modalias; do
            printf "$modalias\n  PCI_DRIVER=$driver\n\n"
        done
        ;;

    modalias-map)
        printf "#define NUL \"\\\\0\"\n"
        printf "static const char modalias_map[] =\n"
        modalias_list $1 | while read driver modalias; do
            printf "\t\"$modalias\" NUL \"$driver\" NUL\n"
        done
        printf "\tNUL;\n"
        printf "#undef NUL\n"
        ;;

    *)
        echo "usage: $0 headers-location hwdb | modalias-map"
        ;;
esac
