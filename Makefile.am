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

dist_man_MANS = weborf.1 weborf.conf.5

init_SCRIPTS = daemon/weborf

sysconf_DATA = weborf.conf

doc_DATA = \
    CHANGELOG \
    COPYING \
    Credits \
    README \
    TODOlist
