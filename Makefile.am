AM_CFLAGS = -Wextra -Wall

SUBDIRS = cgi_wrapper


bin_PROGRAMS = weborf
weborf_SOURCES = \
    auth.c \
    base64.c \
    buffered_reader.c \
    cachedir.c \
    cgi.c \
    configuration.c \
    dict.c \
    http.c \
    instance.c \
    listener.c \
    mime.c \
    mtime_update.c \
    myio.c \
    mynet.c \
    mypoll.c \
    mystring.c \
    queue.c \
    utils.c \
    webdav.c
    

EXTRA_DIST = \
    auth.h \
    buffered_reader.h \
    cgi.h \
    configuration.h \
    dict.h \
    http.h \
    instance.h \
    mime.h \
    mynet.h \
    types.h \
    webdav.h \
    base64.h \
    cachedir.h \
    embedded_auth.h \
    listener.h \
    mtime_update.h \
    mtime_update_fake.c \
    mtime_update_inotify.c \
    myio.h \
    mypoll.h \
    mystring.h \
    queue.h \
    utils.h \
    examples \
    daemon \
    weborf.conf \
    qweborf/main.ui \
    qweborf/nhelper.py \
    qweborf/whelper.py \
    qweborf/qweborf.py \
    qweborf/qweborf \
    qweborf/qweborf.1 \
    qweborf/qweborf.desktop \
    qweborf.setup.py
 
dist-hook:
	rm -rf `find $(distdir)/ -name .svn`

dist_man_MANS = weborf.1 weborf.conf.5

init_SCRIPTS = daemon/weborf

sysconf_DATA = weborf.conf

docdir = ${datadir}/doc/${PACKAGE}

dist_doc_DATA = \
    CHANGELOG \
    COPYING \
    Credits \
    README \
    TODOlist 
