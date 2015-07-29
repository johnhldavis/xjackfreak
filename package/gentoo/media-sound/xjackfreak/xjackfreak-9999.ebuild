# Copyright 1999-2006 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5
inherit eutils git-2
IUSE=""
DESCRIPTION="GNU/Linux Jackd audio frequency analyser & display."
HOMEPAGE="https://github.com/johnhldavis/xjackfreak"
EGIT_REPO_URI="https://github.com/johnhldavis/${PN}.git"

LICENSE="GPL-3"
KEYWORDS="x86"
SLOT="0"

DEPEND=">=media-sound/jack-audio-connection-kit-0.116.1
	x11-libs/libX11"

src_install() {
	make DESTDIR=${D} install || die
}

