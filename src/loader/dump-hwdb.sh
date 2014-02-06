#!/bin/sh

set -e

PROP_NAME=DRI_DRIVER

while read vendor driver; do
    pci_id_file=$1/${driver}_pci_ids.h
    if ! test -r $pci_id_file; then
        printf "pci:v%08X*bc03*\n $PROP_NAME=$driver\n\n" $vendor
        continue
    fi

    gcc -E $pci_id_file |
    while IFS=' (,' read c id rest; do
        test -z "$id" && continue
        printf "pci:v%08Xd%08X*\n $PROP_NAME=$driver\n\n" $vendor $id
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
