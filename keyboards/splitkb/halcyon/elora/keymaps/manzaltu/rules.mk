# Halcyon module support (users/halcyon_modules). The module is selected per build:
#   -e HLC_TFT_DISPLAY=1      left half (TFT display module)
#   -e HLC_CIRQUE_TRACKPAD=1  right half (Cirque trackpad module)
# See qmk.json in the repository root for the two build targets.
USER_NAME := halcyon_modules

ENCODER_MAP_ENABLE = yes
TAP_DANCE_ENABLE = yes
CAPS_WORD_ENABLE = yes
