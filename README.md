eventd
======

eventd, a small daemon to act on remote or local events


Website
-------

To get further information, please visit eventd’s website at:
https://www.eventd.org/

You can also browse man pages online here:
https://www.eventd.org/man/


Build from Git
--------------

To build eventd from Git, you will need some additional dependencies:
- autoconf 2.65 (or newer)
- automake 1.14 (or newer)
- libtool
- pkg-config 0.25 (or newer) or pkgconf 0.2 (or newer)

Make sure to use `git clone --recursive` to fetch submodules too.


Licencing
---------

eventd is distributed under the terms of the [GNU General Public License version 3](https://www.gnu.org/licenses/gpl-3.0.html) (or any later version).
However, some parts of it are distributed under other licences:
- Under the terms of the [GNU Lesser General Public License version 3](https://www.gnu.org/licenses/lgpl-3.0.html) (or any later version):
  - libeventd-event
  - libeventc
  - libeventc-light
  - libeventd-plugin
- Under the terms of the [MIT License](https://opensource.org/licenses/MIT):
  - eventc
  - libnkutils (included submodule)
  - libgwater (included submodule)
