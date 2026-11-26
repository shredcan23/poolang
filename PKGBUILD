pkgname=poolang
pkgver=0.1.0
pkgrel=1
pkgdesc="Custom AST-based interpreted programming language"
arch=('x86_64' 'i686')
license=('MIT')
depends=('gcc-libs')
makedepends=('gcc' 'make')
source=('main.cpp' 'Makefile')
sha256sums=('SKIP' 'SKIP')

build() {
    make
}

package() {
    make DESTDIR="$pkgdir" PREFIX=/usr install
}
