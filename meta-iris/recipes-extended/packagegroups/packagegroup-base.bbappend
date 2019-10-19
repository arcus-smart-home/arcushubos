
# IRIS Change - removed alsa-utils-alsamixer since we do not need it
RDEPENDS_packagegroup-base-alsa = "\
    alsa-utils-alsactl \
    ${VIRTUAL-RUNTIME_alsa-state}"