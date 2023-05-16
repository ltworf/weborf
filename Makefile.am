#SUBDIRS =

bin_PROGRAMS = weborf
weborf_SOURCES = \
    auth.c \
    base64.c \
    buffered_reader.c \
    cachedir.c \
    cgi.c \
    configuration.c \
    instance.c \
    listener.c \
    mime.c \
    myio.c \
    mynet.c \
    mystring.c \
    queue.c \
    utils.c \
    webdav.c

EXTRA_DIST = \
    auth.h \
    buffered_reader.h \
    cgi.h \
    configuration.h \
    instance.h \
    mime.h \
    mynet.h \
    types.h \
    webdav.h \
    base64.h \
    cachedir.h \
    embedded_auth.h \
    listener.h \
    myio.h \
    mystring.h \
    queue.h \
    utils.h \
    examples \
    daemon \
    weborf.conf \
    testsuite/* \
    qweborf/qweborf/__init__.py \
    qweborf/qweborf/main.ui \
    qweborf/qweborf/nhelper.py \
    qweborf/qweborf/whelper.py \
    qweborf/qweborf/qweborf.py \
    qweborf/qweborf/qweborf \
    qweborf/man/qweborf.1 \
    qweborf/integration/qweborf.png \
    qweborf/integration/qweborf.desktop \
    qweborf/integration/qweborf_servicemenu.desktop \
    qweborf/integration/qweborf.metainfo.xml \
    qweborf/setup.py \
    qweborf/Makefile

dist-hook:
	rm -rf `find $(distdir)/ -name .git`

install-data-local:
	install -d echo $(DESTDIR)/etc/weborf.d
	ln -s ../weborf.conf $(DESTDIR)/etc/weborf.d/weborf.conf
	install -D -m644 daemon/weborf.service $(DESTDIR)/lib/systemd/system/weborf.service
	install -D -m644 daemon/weborf@.service $(DESTDIR)/lib/systemd/system/weborf@.service
	install -D daemon/weborf-generator $(DESTDIR)/lib/systemd/system-generators/weborf-generator

install-data-hook:
	chmod a+x $(DESTDIR)$(datadir)/weborf/weborf_launcher

dist_man_MANS = weborf.1 weborf.conf.5

init_SCRIPTS = daemon/weborf

pkgdata_DATA = daemon/weborf_launcher

sysconf_DATA = weborf.conf

dist_doc_DATA = \
    CHANGELOG \
    Credits \
    README.md \
    TODOlist

TESTS = \
    testsuite/version_and_help \
    testsuite/site1http \
    testsuite/site1https \
    testsuite/ip_listener \
    testsuite/etag \
    testsuite/index_file \
    testsuite/range \
    testsuite/cgi \
    testsuite/cachedir \
    testsuite/site1mimetype \
    testsuite/vhost \
    testsuite/functions.sh

