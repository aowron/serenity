#!/usr/bin/env -S bash ../.port_include.sh
port=gnupg
version=2.3.0
useconfigure=true
configopts="--with-libgpg-error-prefix=${SERENITY_BUILD_DIR}/Root/usr/local \
  --with-libgcrypt-prefix=${SERENITY_BUILD_DIR}/Root/usr/local \
  --with-libassuan-prefix=${SERENITY_BUILD_DIR}/Root/usr/local \
  --with-ntbtls-prefix=${SERENITY_BUILD_DIR}/Root/usr/local \
  --with-npth-prefix=${SERENITY_BUILD_DIR}/Root/usr/local \
  --disable-dirmngr"
files="https://gnupg.org/ftp/gcrypt/gnupg/gnupg-${version}.tar.bz2 gnupg-${version}.tar.bz2"
depends="libiconv libgpg-error libgcrypt libassuan npth ntbtls"

pre_configure() {
    export GPGRT_CONFIG="${SERENITY_BUILD_DIR}/Root/usr/local/bin/gpgrt-config"
    export CFLAGS="-L${SERENITY_BUILD_DIR}/Root/usr/local/include"
    export LDFLAGS="-L${SERENITY_BUILD_DIR}/Root/usr/local/lib -lm -liconv"
}

configure() {
    run ./configure --host="${SERENITY_ARCH}-pc-serenity" --build="$($workdir/build-aux/config.guess)" $configopts
}