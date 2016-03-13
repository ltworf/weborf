SUBDIRS = cgi_wrapper

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
    qweborf/__init__.py \
    qweborf/main.ui \
    qweborf/nhelper.py \
    qweborf/whelper.py \
    qweborf/qweborf.py \
    qweborf/qweborf \
    qweborf/qweborf.1 \
    qweborf/qweborf.desktop \
    qweborf/qweborf_servicemenu.desktop \
    qweborf.setup.py
 
dist-hook:
	rm -rf `find $(distdir)/ -name .svn`

dist_man_MANS = weborf.1 weborf.conf.5

init_SCRIPTS = daemon/weborf

sysconf_DATA = weborf.conf

dist_doc_DATA = \
    CHANGELOG \
    COPYING \
    Credits \
    README \
    TODOlist 
