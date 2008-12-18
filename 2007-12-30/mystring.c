/*
Weborf
Copyright (C) 2007  Salvo "LtWorf" Tomaselli

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
 
@author Salvo "LtWorf" Tomaselli <ltworf@galileo.dmi.unict.it> 

 */


#include "mystring.h"
#include <string.h>

/*Questo metodo trova il ?, lo sostituisce con \0 così da terminare li la stringa, 
rimuovendo i parametri che seguono.
Restituisce la posizione del ? così i parametri possono essere recuperati 
Restituisce -1 se il ? non compare*/
int nullParams(char * string) {
	int i;
	int to=strlen(string);
	
	for (i=0;i<to;i++) {
		if (string[i]=='?') {
			string[i]='\0';
			return i;
		}
	}
	return -1;
}




/*
Questa funzione cambia i separatori dei parametri & in \0
Così da creare tante stringhe separate.
Restituisce il numero di parametri
*/
int splitParams(char * string) {
	int params=0;
	int i;
	int to=strlen(string);//La salvo prima
	
	for (i=0;i<to;i++) {
		if (string[i]=='&') {
			string[i]='\0';
			params++;
		}
	}
	return params+1;
}

/*
Restituisce 0 se la prima stringa file ha estensione ext
*/
int estensione(char * file, char * ext) {
	int i;
	int len_ext=strlen(ext);
	int len_file=strlen(file);
	
	for (i=1;i<=len_ext;i++) {
		if (ext[len_ext-i]!=file[len_file-i]) return -1;
	}
	
	return 0;
}
