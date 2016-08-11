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

/**
 * This file contains user defined embedded autentication
 * to avoid the usage of an external daemon for authentication.
 * The use of this, will make things faster and parallel, at the cost
 * of needing to define it at compile time
 *
 * Everything in this file should be between the "#ifdef EMBEDDED_AUTH"
 * and its "#endif" to avoid the compilation of unused methods
 * */

#define EMBEDDED_AUTH

#ifdef EMBEDDED_AUTH

extern weborf_configuration_t weborf_conf;                                      

/**
 * This function is just example code, can be changed or deleted
 */
static int emb_check_password(char *username, char *password) {
    char *user="pp";
    char *pass="pp";
    if (weborf_conf.username) user = weborf_conf.username;
    if (weborf_conf.password) pass = weborf_conf.password;
    if (strncmp(username,user,strlen(user))==0 && strncmp(password,pass,strlen(pass))==0)
        return 0;
    return -1;
}

/**
 * This function will be used if EMBEDDED_AUTH is defined, do not modify its signature!
 *
 * page:       URI of requested page
 * ip_addr:    IP address of the client (it can be an IPv6, depending how weborf is compiled)
 * method:     HTTP method of the request
 * username:   Username, if provided, null otherwise
 * password:   Password, if provided, null otherwise
 * http_param: Headers of the request
 *
 * RETURN VALUE:
 * Return 0 to allow the request, -1 to deny it.
 *
 * NOTES:
 * The actual content of the function must be regarded as an example, modify it according to your
 * own needs.
 * Only use reentrant calls in this function. Weborf is multithreaded.
 */
static int c_auth(char *page, char *ip_addr, char *method, char *username, char *password, char *http_param) {
    return emb_check_password(username,password);
    char *allowed_prefix="::ffff:10.";
    char *foto = "/foto/";

    //Allow anything from 10.*
    if (strncmp(allowed_prefix,ip_addr,strlen(allowed_prefix))==0) return 0;

    //Allow PROPFIND and OPTIONS with authentication (to allow read only webdav from anywhere)
    if ((strncmp(method,"PROPFIND",strlen("PROPFIND"))==0) || (strncmp(method,"OPTIONS",strlen("OPTIONS"))==0))
        return emb_check_password(username,password);

    //Deny all except GET and POST
    else if (!(strncmp(method,"GET",strlen("GET"))==0 || strncmp(method,"POST",strlen("POST"))==0)) {
        return -1;
    }

    //request authentication for photos
    if (strncmp(foto,page,strlen(foto))==0)
        return emb_check_password(username,password);


    return 0;

}

#endif
