/*
Weborf
Copyright (C) 2010  Salvo "LtWorf" Tomaselli

Weborf is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

@author Salvo "LtWorf" Tomaselli <tiposchi@tiscali.it>
*/
#include "options.h"

#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef HAVE_LIBSSL
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

#include "configuration.h"
#include "types.h"
#include "utils.h"
#include "cachedir.h"
#include "auth.h"

weborf_configuration_t weborf_conf = {
    .tar_directory=false,
    .is_inetd=false,
    .virtual_host = false,
    .exec_script = true,
    .ip = NULL,
    .port = PORT,
    .basedir=BASEDIR,
    .uid = ROOTUID,
    .gid = ROOTGID,
    .daemonize = false,
#ifdef HAVE_LIBSSL
    .sslctx = NULL,
#endif

#ifdef SEND_MIMETYPES
    .send_content_type = false,
#endif
};

/**
 * Enables sending mime types in response to GET requests
 * or prints an error and exits if the support was not
 * compiled
 * */
static void configuration_enable_sending_mime() {
#ifdef SEND_MIMETYPES
    weborf_conf.send_content_type=true;
#else
    fprintf(stderr,"Support for MIME is not available\n");
    exit(19);
#endif
}


/**
Sets the base dir, making sure that it is really a directory.
 */
static void configuration_set_basedir(char * bd) {
    struct stat stat_buf;
    stat(bd, &stat_buf);

    if (!S_ISDIR(stat_buf.st_mode)) {
        //Not a directory
        printf("%s must be a directory\n",bd);
        exit(1);
    }
    weborf_conf.basedir = bd;
}

/**
 * Sets default CGI configuration,
 * run .php and .py as CGI
 * */
static void configuration_set_default_CGI() {
    weborf_conf.cgi_paths.len=4;
    weborf_conf.cgi_paths.data[0]=".php";
    weborf_conf.cgi_paths.data[1]=CGI_PHP;
    weborf_conf.cgi_paths.data[2]=".py";
    weborf_conf.cgi_paths.data[3]=CGI_PY;
    weborf_conf.cgi_paths.data_l[0]=strlen(".php");
    weborf_conf.cgi_paths.data_l[1]=strlen(CGI_PHP);
    weborf_conf.cgi_paths.data_l[2]=strlen(".py");
    weborf_conf.cgi_paths.data_l[3]=strlen(CGI_PY);
}

/**
 * Sets the default index file
 * */
static void configuration_set_default_index() {
    weborf_conf.indexes[0] = INDEX;
    weborf_conf.indexes_l = 1;
}

static void configuration_set_cgi(char *optarg) {
    int i = 0;
    weborf_conf.cgi_paths.len = 1; //count of indexes
    weborf_conf.cgi_paths.data[0] = optarg; //1st one points to begin of param
    while (optarg[i++] != 0) { //Reads the string
        if (optarg[i] == ',') {
            optarg[i++] = 0; //Nulling the comma
            //Increasing counter and making next item point to char after the comma
            weborf_conf.cgi_paths.data[weborf_conf.cgi_paths.len++] = &optarg[i];
            if (weborf_conf.cgi_paths.len == MAXINDEXCOUNT) {
                perror("Too much cgis, change MAXINDEXCOUNT in options.h to allow more");
                exit(6);
            }
        }
    }

    for (i=0; i<weborf_conf.cgi_paths.len; i++) {
        weborf_conf.cgi_paths.data_l[i]=strlen(weborf_conf.cgi_paths.data[i]);
    }

}

static void configuration_set_index_list(char *optarg) { //Setting list of indexes
    int i = 0;
    weborf_conf.indexes_l = 1; //count of indexes
    weborf_conf.indexes[0] = optarg; //1st one points to begin of param
    while (optarg[i++] != 0) { //Reads the string

        if (optarg[i] == ',') {
            optarg[i++] = 0; //Nulling the comma
            //Increasing counter and making next item point to char after the comma
            weborf_conf.indexes[weborf_conf.indexes_l++] = &optarg[i];
            if (weborf_conf.indexes_l == MAXINDEXCOUNT) {
                perror("Too much indexes, change MAXINDEXCOUNT in options.h to allow more");
                exit(6);
            }
        }
    }

}

static void configuration_set_virtualhost(char *optarg) {
    weborf_conf.virtual_host = true;

    int i = 0;
    char *virtual = optarg; //1st one points to begin of param

    while (optarg[i++] != 0) { //Reads the string
        if (optarg[i] == ',') {
            optarg[i++] = 0; //Nulling the comma
            putenv(virtual);
            virtual = &optarg[i];

        }
    }
    putenv(virtual);
}

#ifdef HAVE_LIBSSL
static void init_ssl(char *certificate, char* key) {
    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_all_algorithms();

    weborf_conf.sslctx = SSL_CTX_new( TLS_server_method());
    if (!weborf_conf.sslctx) {
        printf("Error: %s\n", ERR_error_string(ERR_get_error(), NULL));
        abort();
    }

    SSL_CTX_set_options(weborf_conf.sslctx, SSL_OP_SINGLE_DH_USE);
    if (SSL_CTX_use_certificate_file(weborf_conf.sslctx, certificate, SSL_FILETYPE_PEM) != 1) {
        printf("Error: %s\n", ERR_error_string(ERR_get_error(), NULL));
        abort();
    }
    if (SSL_CTX_use_PrivateKey_file(weborf_conf.sslctx, key, SSL_FILETYPE_PEM) != 1) {
        printf("Error: %s\n", ERR_error_string(ERR_get_error(), NULL));
        abort();
    }
}
#endif

void configuration_load(int argc, char *argv[]) {
    configuration_set_default_CGI();
    configuration_set_default_index();


    int c; //Identify the readed option
    int option_index = 0;
    char *certificate = NULL;
    char *key = NULL;

    //Declares options
    struct option long_options[] = {
        {"version", no_argument, 0, 'v'},
        {"caps", no_argument, 0, 'k'},
        {"help", no_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {"ip", required_argument, 0, 'i'},
        {"uid", required_argument, 0, 'u'},
        {"gid", required_argument, 0, 'g'},
        {"daemonize", no_argument, 0, 'd'},
        {"basedir", required_argument, 0, 'b'},
        {"index", required_argument, 0, 'I'},
        {"auth", required_argument, 0, 'a'},
        {"virtual", required_argument, 0, 'V'},
        {"moo", no_argument, 0, 'M'},
        {"noexec", no_argument, 0, 'x'},
        {"cgi", required_argument, 0, 'c'},
        {"cache", required_argument, 0, 'C'},
        {"mime", no_argument,0,'m'},
        {"inetd", no_argument,0,'T'},
        {"tar", no_argument,0,'t'},
#ifdef HAVE_LIBSSL
        {"cert", required_argument, 0, 'S'},
        {"key", required_argument, 0, 'K'},
#endif
        {0, 0, 0, 0}
    };


    while (1) { //Block to read command line

        option_index = 0;

        //Reading one option and telling what options are allowed and what needs an argument
        c = getopt_long(
            argc,
            argv,
#ifdef HAVE_LIBSSL
            "ktTMmvhp:i:I:u:g:dxb:a:V:c:C:S:",
#else
            "ktTMmvhp:i:I:u:g:dxb:a:V:c:C:",
#endif
            long_options,
            &option_index
        );

        //If there are no options it continues
        if (c == -1)
            break;

        switch (c) {
        case 'k':
            print_capabilities();
            break;
        case 't':
            weborf_conf.tar_directory=true;
            break;
        case 'T':
            weborf_conf.is_inetd=true;
            break;
        case 'C':
            cache_init(optarg);
            break;
        case 'm':
            configuration_enable_sending_mime();
            break;
        case 'c':
            configuration_set_cgi(optarg);
            break;
        case 'V':
            configuration_set_virtualhost(optarg);
            break;
        case 'I':
            configuration_set_index_list(optarg);
            break;
        case 'b':
            configuration_set_basedir(optarg);
            break;
        case 'x':
            weborf_conf.exec_script = false;
            break;
        case 'v':   //Show version and exit
            version();
            break;
        case 'h':   //Show help and exit
            help();
            break;
        case 'p':
            weborf_conf.port = optarg;
            break;
        case 'i':
            weborf_conf.ip = optarg;
            break;
        case 'u':
            weborf_conf.uid = strtol(optarg, NULL, 0);
            break;
        case 'g':
            weborf_conf.gid = strtol(optarg, NULL, 0);
            break;
        case 'd':
            weborf_conf.daemonize = true;
            break;
        case 'a':
            auth_set_socket(optarg);
            break;
        case 'M':
            moo();
            break;
#ifdef HAVE_LIBSSL
        case 'S':
            certificate = optarg;
            break;
        case 'K':
            key = optarg;
            break;
#endif
        default:
            exit(19);
        }

    }

#ifdef HAVE_LIBSSL
    if (certificate || key) {
        if (weborf_conf.tar_directory) {
            fprintf(stderr, "Sending directories as tar is not supported while SSL is in use.\n");
            exit(19);

        }
        init_ssl(certificate, key);
    }
#endif
}
