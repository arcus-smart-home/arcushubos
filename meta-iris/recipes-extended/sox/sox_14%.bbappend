
# Strip down package, we just need MP3 and AMR related support
DEPENDS = "libtool"
PACKAGECONFIG = "alsa mad amrnb amrwb"

EXTRA_OECONF = " \
    --without-png \
    --without-flac \
    --without-oggvorbis \
    --without-sndfile \
    "