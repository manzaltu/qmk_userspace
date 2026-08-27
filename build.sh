#!/usr/bin/env bash
# Build the manzaltu keymap for both halves of the Halcyon Elora rev2 and flash
# them one after the other (right half first, then left).
#
#   ./build.sh            build both halves, then flash right and left
#   ./build.sh build      build only
#   ./build.sh flash      flash only (right, then left), using the existing .uf2 files
#   ./build.sh flash left | right
#                         flash a single half
#
# The module is chosen at compile time, so each half gets its own firmware:
# left = TFT display module, right = Cirque trackpad module.
set -euo pipefail
cd "$(dirname "$0")"

KEYBOARD=splitkb/halcyon/elora/rev2
KEYMAP=manzaltu
LEFT_TARGET=splitkb_halcyon_elora_rev2_${KEYMAP}_left_display
RIGHT_TARGET=splitkb_halcyon_elora_rev2_${KEYMAP}_right_trackpad
BOOT_LABEL=/dev/disk/by-label/RPI-RP2

build() {
    qmk userspace-compile
}

# Wait for the RP2040 bootloader drive, mount it if needed and copy the firmware.
flash_half() {
    local side=$1 target=$2 file
    file=$PWD/$target.uf2
    [[ -f $file ]] || { echo "missing $file - run './build.sh build' first" >&2; exit 1; }

    echo
    echo ">>> $side half: double-tap the reset button to enter the bootloader"
    echo "    (waiting for the RPI-RP2 drive, Ctrl-C to abort)"
    until [[ -e $BOOT_LABEL ]]; do sleep 1; done

    local dev mnt
    dev=$(readlink -f "$BOOT_LABEL")
    mnt=$(awk -v d="$dev" '$1 == d { print $2 }' /proc/mounts)
    if [[ -z $mnt ]]; then
        udisksctl mount -b "$dev" > /dev/null
        mnt=$(awk -v d="$dev" '$1 == d { print $2 }' /proc/mounts)
    fi
    [[ -n $mnt ]] || { echo "could not mount $dev" >&2; exit 1; }

    echo "    copying $target.uf2 -> $mnt"
    cp "$file" "$mnt/"
    sync

    # The half reboots into the new firmware as soon as the copy is complete.
    for _ in $(seq 20); do [[ -e $BOOT_LABEL ]] || break; sleep 1; done
    if [[ -e $BOOT_LABEL ]]; then
        echo "    warning: bootloader drive still present, flashing may have failed" >&2
    else
        echo "    $side half flashed and rebooted"
    fi
}

flash() {
    case ${1:-both} in
        right) flash_half right "$RIGHT_TARGET" ;;
        left)  flash_half left  "$LEFT_TARGET" ;;
        both)  flash_half right "$RIGHT_TARGET"; flash_half left "$LEFT_TARGET" ;;
        *) echo "unknown half: $1 (left|right)" >&2; exit 1 ;;
    esac
}

case ${1:-all} in
    build) build ;;
    flash) flash "${2:-both}" ;;
    all)   build; flash both ;;
    *) sed -n '2,13p' "$0"; exit 1 ;;
esac
