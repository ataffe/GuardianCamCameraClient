set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(PI_SYSROOT "/sysroot" CACHE PATH "pi-sysroot")
set(TRIPLE aarch64-linux-gnu)

set(CMAKE_C_COMPILER   ${TRIPLE}-gcc)
set(CMAKE_CXX_COMPILER ${TRIPLE}-g++)

set(CMAKE_SYSROOT      ${PI_SYSROOT})
set(CMAKE_FIND_ROOT_PATH ${PI_SYSROOT})

set(OPENSSL_ROOT_DIR ${PI_SYSROOT}/usr)
set(OPENSSL_INCLUDE_DIR ${PI_SYSROOT}/usr/include)
set(OPENSSL_CRYPTO_LIBRARY ${PI_SYSROOT}/usr/lib/${TRIPLE}/libcrypto.so)
set(OPENSSL_SSL_LIBRARY ${PI_SYSROOT}/usr/lib/${TRIPLE}/libssl.so)

set(CPR_USE_SYSTEM_CURL ON CACHE BOOL "" FORCE)

# Zero 2 W is a Cortex-A53
set(CMAKE_C_FLAGS_INIT   "-mcpu=cortex-a53")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-a53")

# Helps the linker resolve transitive deps (libA needs libB) inside the sysroot
set(CMAKE_EXE_LINKER_FLAGS_INIT
        "-Wl,-rpath-link,${PI_SYSROOT}/usr/lib/${TRIPLE}:${PI_SYSROOT}/lib/${TRIPLE}")

# Find executables on the host, but headers and libraries ONLY in the sysroot
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config must be redirected too, or it hands you host paths
set(ENV{PKG_CONFIG_DIR} "")
set(ENV{PKG_CONFIG_LIBDIR}
        "${PI_SYSROOT}/usr/lib/${TRIPLE}/pkgconfig:${PI_SYSROOT}/usr/lib/pkgconfig:${PI_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${PI_SYSROOT}")