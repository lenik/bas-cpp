# Version is injected by packaging/rpm/Makefile via `zfr version`.
# RPM Version cannot contain '-'; use `zfr version -r` (hyphens → '_').
# srcversion is the unsanitized Meson/git version and names the tarball.
%{!?version:%global version 0.0.0}
%{!?srcversion:%global srcversion %{version}}

Name:           bas-cpp
Version:        %{version}
Release:        1%{?dist}
Summary:        C++ core library (BAS stack)

License:        AGPL-3.0-or-later
URL:            http://uni.bodz.net/lib/libbas-cpp
Packager:       Lenik (谢继雷) <lenik@bodz.net>
Source0:        %{name}-%{srcversion}.tar.xz

BuildRequires:  meson
BuildRequires:  ninja-build
BuildRequires:  libbas-c-dev
BuildRequires:  libboost-dev
BuildRequires:  libboost-system-dev
BuildRequires:  libext2fs-dev
BuildRequires:  libglib2.0-dev
BuildRequires:  libicu-dev
BuildRequires:  libcurl4-openssl-dev
BuildRequires:  libncurses-dev
BuildRequires:  libssl-dev
BuildRequires:  zlib1g-dev
BuildRequires:  asciidoctor

%description
Shared library for bas-cpp, built on the BAS stack (bas-c, Boost, ICU,
libcurl, etc.). Provides unified I/O, volume abstraction, and helpers
that are reused by command-line tools and other applications.

%prep
%setup -q -n %{name}-%{srcversion}

%build
meson setup build \
    --prefix=%{_prefix} \
    --bindir=%{_bindir} \
    --datadir=%{_datadir} \
    --mandir=%{_mandir} \
    --sysconfdir=%{_sysconfdir} \
    --localstatedir=%{_localstatedir} \
    --buildtype=plain
meson compile -C build

%install
meson install -C build --destdir=%{buildroot}

%files
%{_bindir}/tanks_game
%{_datadir}/bash-completion/completions/vols
%{_mandir}/man1/vols.1*
%{_datadir}/bas-cpp/
%{_includedir}/*
%{_datadir}/locale/*/LC_MESSAGES/bas_cpp.mo
%{_datadir}/doc/bas-cpp/

%changelog
* Thu Aug 20 2026 Lenik (谢继雷) <lenik@bodz.net>
- Align spec with debian/control (Meson, AGPL-3.0-or-later).
- Version comes from `zfr version`, the same method meson.build uses.
