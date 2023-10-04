AC_INIT([weborf], [1.0], [tiposchi@tiscali.it])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_SRCDIR([queue.c])
AC_CONFIG_HEADERS([config.h])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AM_MAINTAINER_MODE


AC_PROG_CC


AC_SUBST([cgibindir], [${libdir}/cgi-bin])
AC_SUBST([initdir], [${sysconfdir}/init.d])

AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h string.h strings.h sys/file.h sys/socket.h syslog.h unistd.h])
AC_CHECK_FUNCS([alarm inet_ntoa localtime_r memmove memset mkdir putenv rmdir setenv socket strstr strtol strtoul ftruncate strrchr])

AC_SYS_LARGEFILE


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
AC_CHECK_LIB([magic], [magic_load])
AC_CHECK_LIB([crypto], [RAND_add])
AC_CHECK_LIB([ssl], [SSL_new])

#AC_CONFIG_HEADERS([config.h options.h])

AC_CONFIG_FILES([Makefile
    options.h
])


AC_OUTPUT
