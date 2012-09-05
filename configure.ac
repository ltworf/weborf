AC_INIT([weborf], [0.14], [tiposchi@tiscali.it],[weborf])
AC_DEFINE([PACKAGE_URL], ["http://galileo.dmi.unict.it/wiki/weborf/"],[Define to the home page for this package.])



AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_SRCDIR([queue.c])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_TARGET

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])
AM_INIT_AUTOMAKE([foreign -Wall -Werror ])
AM_MAINTAINER_MODE



AC_PROG_CC


AC_SUBST([cgibindir], [${libdir}/cgi-bin])
AC_SUBST([initdir], [${sysconfdir}/init.d])

AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h string.h strings.h sys/file.h sys/socket.h syslog.h unistd.h sys/time.h utime.h sys/time.h])
AC_CHECK_HEADERS([sys/resource.h locale.h])
AC_CHECK_FUNCS([alarm inet_ntoa localtime_r memmove memset mkdir putenv rmdir setenv socket strstr strtol strtoul strtoull ftruncate strrchr utime setrlimit])
AC_CHECK_FUNCS([inotify_init setlocale strdup pread dup2])
AC_CHECK_FUNCS([epoll_create epoll_ctl epoll_wait])




AC_SYS_LARGEFILE


AC_C_CONST
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_FUNC_UTIME_NULL
AC_FUNC_REALLOC
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_STRUCT_TM 
AC_TYPE_SIGNAL
AC_HEADER_STDBOOL
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UID_T
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_C_INLINE

AC_CHECK_LIB([pthread], [pthread_create], , \
    AC_MSG_ERROR([libpthread is needed])
)
AC_CHECK_LIB([magic], [magic_load], , )

#AC_CONFIG_HEADERS([config.h options.h])

AC_CONFIG_FILES([Makefile
    cgi_wrapper/Makefile
    options.h
])


AC_OUTPUT
