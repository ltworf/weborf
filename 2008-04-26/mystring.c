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
	int i=0;
	while (string[i]!=0) {
		if (string[i]=='?') {
			string[i]='\0';
			return i;
		}
		i++;
	}
	return -1;
}


/**
Questa funzione rimpiazza, all'interno della stringa string
la sottostringa substr con il carattere with.

Questa funzione opera in-place, non crea copie della stringa e non occupa memoria addizionale.
*/
void strReplace(char* string,char* substr, char with) {
	char* pointer;
	int substrlen=strlen(substr);//Dato che si leggerà questo valore più di una volta si ottimizza
	
	while ((pointer=strstr (string, substr))!=NULL) {
		delChar(pointer,0,substrlen-1);
		pointer[0]=with;
	}
}

/**
Questa funzione elimina n caratteri dalla stringa a partire dalla posizione pos.
In caso si richieda la rimozione di più caratteri di quelli che sono presenti, oppure
nel caso in cui si iniziasse l'eliminazione da un carattere verso la fine della stringa che 
non sia seguito da un numero sufficiente di caratteri, verrà restituita la stringa senza modifiche.


Questa funzione modifica la stringa senza creare copie.
*/
void delChar(char* string,int pos, int n) {
	if (strlen (string) < n + pos) { //La stringa non è abbastanza lunga 
		return; 
	}

	char *c1, *c2; 	
	for (c1 = string + pos, c2 = c1 + n; *c2 != 0; c1++, c2++) {
		*c1=*c2; 
	}
	*c1=0;
}



/**
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

/**
Returns 0 if file ends with ext
-1 otherwise
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
