
# Remove some features to trim size

EXTRA_OECONF += " \
    --disable-aload \
    --disable-rawmidi \
    --disable-seq \
    --disable-ucm \
    --disable-topology \
    --disable-alisp \
    --disable-old-symbols \
    --disable-python \
"
