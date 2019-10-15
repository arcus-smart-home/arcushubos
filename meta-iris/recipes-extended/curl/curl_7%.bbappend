
# Replace gnutls with openssl to avoid a lot of added code in build

PACKAGECONFIG ??= "${@bb.utils.filter('DISTRO_FEATURES', 'ipv6', d)} ssl proxy threaded-resolver zlib"

EXTRA_OECONF += " \
    --with-ssh \
    --without-gnutls \
"
