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

/*
 * This file contains user defined embedded autentication
 * to avoid the usage of an external daemon for authentication.
 * The use of this, will make things faster and parallel, at the cost
 * of needing to define it at compile time
 * */

//#define EMBEDDED_AUTH

#ifdef EMBEDDED_AUTH
int c_auth(char *page, char *ip_addr, char *method, char *username, char *password, char *http_param) {
    char *allowed_prefix="::ffff:10.";
    char *foto = "/foto/";
        
    char *user="gentoo";
    char *pass="lalalala";

    //Allow anything from 10.*
    if (strncmp(allowed_prefix,ip_addr,strlen(allowed_prefix))==0) return 0;

    //ALLOW wanted methods
    if (!(strncmp(method,"GET",strlen("GET"))==0 || strncmp(method,"POST",strlen("POST"))==0)) {
        return -1;
    }

    //request authentication for photos
    if (strncmp(foto,page,strlen(foto))==0) {
        if (strncmp(username,user,strlen(user))==0 && strncmp(password,pass,strlen(pass))==0)
            return 0;
        return -1;
    }

    return 0;

}
#endif
