
# Remove some features to trim size

EXTRA_OECONF += " \
    --disable-bat \
    --disable-alsatest \
    --disable-alsaloop \
    --disable-alsamixer \
    --disable-xmlto \
    --disable-rst2man \
    --disable-largefile \
"

# Remove test .wav files
FILES:alsa-utils-speakertest = "${bindir}/speaker-test"

INSANE_SKIP:${PN} = "installed-vs-shipped"