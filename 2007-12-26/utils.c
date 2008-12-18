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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "utils.h"

/** Restituisce 0 se il file specificato esiste */
int file_exists(char * file) {
	int fp= open(file,O_RDONLY);
	if(fp>=0 ) {// esiste
		close(fp);
	} else {//non esiste
		return -1;
	}
	return 0;
}

/**
Metodo non usato per ottenere la dimensione del file
Volendo si può inserire la dimensione nell'header così il browser può mostrare l'avanzamento
*/
int getFileSize(int fn) {
	
	struct stat buf;
	if (fstat(fn, &buf) < 0){
		return -1;
	}
	return (buf.st_size);
}

/**
Trova il primo carattere nullo e restituisce il puntatore al carattere successivo.
*/
char* findNext(char* str) {
	return (char *)((int)str+strlen(str)+2);
}
