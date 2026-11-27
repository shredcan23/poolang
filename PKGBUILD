pkgname=poolang
pkgver=0.2.0
pkgrel=1
pkgdesc="Custom AST-based interpreted programming language with built-in graphics libraries"
arch=('x86_64' 'i686' 'aarch64')
license=('MIT')
depends=('gcc-libs' 'libx11')
makedepends=('gcc' 'make')
source=(
    'Makefile'
    'src/main.cpp'
    'libs/poographic.hpp'
    'libs/poographic.cpp'
    'libs/poographicl.hpp'
    'libs/poographicl.cpp'
)
sha256sums=('SKIP' 'SKIP' 'SKIP' 'SKIP' 'SKIP' 'SKIP')

build() {
    mkdir -p src libs
    cp -f "$srcdir/main.cpp" src/
    cp -f "$srcdir/poographic.hpp" "$srcdir/poographic.cpp" \
          "$srcdir/poographicl.hpp" "$srcdir/poographicl.cpp" libs/
    make
}

package() {
    make DESTDIR="$pkgdir" PREFIX=/usr install
}
