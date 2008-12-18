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
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include "options.h"

/**
Questa funzione è identica a strcat, tranne per il fatto che controlla che la dimensione
del risultato non superi la massima dimensione consentita.

In questo caso restituisce la prima stringa, senza concatenare niente.
*/
char * strcat_chk_size(char* to, char* from, int maxsize) {
	if (strlen(to)+strlen(from)>maxsize) return to;//Se troppo lungo non concatena
	return strcat(to,from);
}
	

/**
Questa funzione effettua la scansione della directory dir
mettendo all'interno della stringa html una pagina html che contiene gli elementi
della directory dir, con i link.

La stringa html deve essere allocata dal chiamante.
bufsize contiene la dimensione dello spazio allocato per html, per prevenire buffer overflow.
*/
int list_dir (char* dir,char * html,unsigned int bufsize) {
	int maxsize=bufsize-1;//Dimensione massima per la stringa
	struct dirent *ep;//Struct con le proprietà dell'elemento
	DIR *dp = opendir (dir);//Apre la directory
	
	if (dp != NULL) {
		html=strcat_chk_size(html,HTMLHEAD,maxsize);//Mette l'header html nella stringa
		
		char buf[INBUFFER];//Buffer per contenere l'elemento
		char path[INBUFFER];//Buffer per contenere il path completo dell'elemento
		
		while ((ep = readdir (dp))) {//Scorre gli elementi
			sprintf(path,"%s%s",dir,ep->d_name);
						
			int f_mode=fileIsA(path);//Ottiene il mode dell'elemento
			if (S_ISREG(f_mode)) {
				//Crea la stringa per il file
				sprintf (buf,"<a href=\"%s\">%s</a><br>\n",ep->d_name,ep->d_name);
			} else if (S_ISDIR(f_mode)) {//Se l'elemento è una directory
				//Crea la stringa per il file
				sprintf (buf,"&nbsp;&nbsp;&nbsp;<a href=\"%s/\">%s/</a><br>\n",ep->d_name,ep->d_name);
				
			}
			
			//La inserisce nell'html
			html=strcat_chk_size(html,buf,maxsize);
		}
			
		closedir (dp);
	} else {//Errore aprendo la directory
		return -1;
	}
	
	//Aggiunge la parte finale dell'html
	html=strcat_chk_size(html,HTMLFOOT,maxsize);
	
	return 0;
}



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
*/
int getFileSize(int fn) {
	
	struct stat buf;
	if (fstat(fn, &buf) < 0){
		return -1;
	}
	return (buf.st_size);
}

/**
Restituisce il mode del file passato come parametro. 
O -1 in caso di errore
*/
int fileIsA(char* file) {
	
	struct stat buf;
	if (stat(file, &buf) < 0){
		return -1;
	}
	return (buf.st_mode);
}

/**
Trova il primo carattere nullo e restituisce il puntatore al carattere successivo.
*/
char* findNext(char* str) {
	return (char *)((int)str+strlen(str)+2);
}

void invalidOption(char *option) {
	printf("Invalid option %s\n\n",option);
	help();
}

void version() {
printf ("Weborf 0.03\n");
printf ("Copyright (C) 2007 LSD: LSD Sicilian Developers.\n");
printf ("This is free software.  You may redistribute copies of it under the terms of\n");
printf ("the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n");
printf ("There is NO WARRANTY, to the extent permitted by law.\n\n");

printf ("Written by Salvo 'LtWorf' Tomaselli and Salvo Rinaldi.\n");
printf ("Synchronized queue by Prof. Giuseppe Pappalardo.\n");
exit(0);
}

/**
Stampa l'help del programma
 */
void help() {

	printf("\tUsage: weborf [OPTIONS]\n");
	printf("\tStart the weborf webserver\n\n");

	printf("  -p, --port	followed by port number to listen\n");
	printf("  -i, --ip	followed by IP address to listen (dotted format)\n");
	printf("  -b, --basedir	followed by absolute path of basedir\n");
	printf("  -h, --help	display this help and exit\n");
	printf("  -v, --version	print program version\n\n");

	printf("Default IP address is %s\n",LOCIP);
	printf("Default port is %s\n",PORT);
	printf("Default base directory is %s\n\n",BASEDIR);
	
	printf("Report bugs to <ltworf@galileo.dmi.unict.it>\n");
	exit(0);
}

void moo() {
	printf(" _____________________________________\n");
	printf("< Weborf ha i poteri della supermucca >\n");
	printf(" -------------------------------------\n");
	printf("        \\   ^__^\n");
	printf("         \\  (oo)\\_______\n");
	printf("            (__)\\       )\\/\\\n");
	printf("                ||----w |\n");
	printf("                ||     ||\n");
	exit (0);
}
