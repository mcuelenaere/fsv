# Generated automatically from fsv.spec.in by configure.

%define prefix /usr

Name: fsv
Version: 0.9
Release: 1
Group: X11/Utilities
Copyright: LGPL
URL: http://fox.mit.edu/skunk/soft/fsv/
Summary: fsv - 3D File System Visualizer

Requires: gtk+ >= 1.2.1
Source: http://fox.mit.edu/skunk/soft/src/fsv-0.9.tar.gz
BuildRoot: /tmp/fsv-0.9-root

%description
fsv (pronounced effessvee) is a file system visualizer in cyberspace. It
lays out files and directories in three dimensions, geometrically
representing the file system hierarchy to allow visual overview and
analysis. fsv can visualize a modest home directory, a workstation's hard
drive, or any arbitrarily large collection of files, limited only by the
host computer's memory and hardware constraints.

%prep

%setup

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{prefix} --with-doc-dir=$RPM_DOC_DIR/fsv-0.9
make

%install
rm -rf $RPM_BUILD_ROOT
make install prefix=$RPM_BUILD_ROOT%{prefix} docdir=`pwd`/doc.rpm
mkdir -p $RPM_BUILD_ROOT/etc/X11/wmconfig
install -m 0644 fsv.wmconfig $RPM_BUILD_ROOT/etc/X11/wmconfig/fsv

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc doc.rpm/*
/etc/X11/wmconfig/fsv
%{prefix}/bin/fsv
## %{prefix}/share/locale/*/*/*
