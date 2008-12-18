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
#include "mystring.h"

/**
This function works like strcat, but this one does size check too.

If the string to concat is too big, and would bring to a buffer overflow, the function will do nothing.
*/
char * strcat_chk_size(char* to, char* from, int maxsize) {
	if (strlen(to)+strlen(from)>maxsize) return to;//Se troppo lungo non concatena
	return strcat(to,from);
}
	

/**
This function reads the directory dir, putting inside the html string an html page with links to all the files within the directory.

Buffer for html must be allocated by the calling function.
bufsize is the size of the buffer allocated for html. To avoid buffer overflows.
*/
int list_dir (char* dir,char * html,unsigned int bufsize,bool parent) {
	int maxsize=bufsize-1;//String's max size
	struct dirent *ep;//File's property
	DIR *dp = opendir (dir);//Open dir
	
	if (dp != NULL) {//Open succesfull
		html=strcat_chk_size(html,HTMLHEAD,maxsize);//Puts html header within the string
		
		char buf[INBUFFER];//Buffer to contain the element
		char path[INBUFFER];//Buffe to contain element's absolute path
		
		//Specific header table)
		strcat_chk_size(html,"<table><tr><td></td><td>Name</td><td>Size</td></tr>",maxsize);
		
		while ((ep = readdir (dp))) {//Cycles trough dir's elements
			sprintf(path,"%s/%s",dir,ep->d_name);
			
			struct stat f_prop;//File's property
			stat(path, &f_prop);
			int f_mode=f_prop.st_mode;//Get's file's mode
			
			if (S_ISREG(f_mode)) {//Regular file
				
				//Table row for the file
				sprintf (buf,
				"<tr><td>f</td><td><a href=\"%s\">%s</a></td><td>%dkB</td></tr>\n",
				ep->d_name,ep->d_name,(int)(f_prop.st_size/1024));
				
			} else if (S_ISDIR(f_mode)) {//Directory
				bool print=true;
				//Table row for the dir
				if (ep->d_name[0]=='.' && ep->d_name[1]=='.' && ep->d_name[2]=='\0') {//Parent entry
					//If showing parent entry is forbidden, doesn't show it
					if (parent==false) print=false;
				} else if (ep->d_name[0]=='.' && ep->d_name[1]=='\0') {//Self entry
					//doesn't show it
					print=false;
				}
				
				
				if (print) {
					sprintf (buf,
					"<tr><td>d</td><td><a href=\"%s/\">%s/</a></td><td>-</td></tr>\n",
					ep->d_name,ep->d_name);
				} else {
					buf[0]='\0';
				}
				
				
			}
			
			//Insert row into the html
			html=strcat_chk_size(html,buf,maxsize);
		}
		
		//Table's footer
		strcat_chk_size(html,"</table>",maxsize);
		
		closedir (dp);
		
		
	} else {//Error opening the directory
		return -1;
	}
	
	//Adds html footer
	html=strcat_chk_size(html,HTMLFOOT,maxsize);
	
	return 0;
}



/** 
Returns 0 if the specified file exists
*/
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
Used to obtain file size from its descriptor
*/
int getFileSize(int fn) {
	
	struct stat buf;
	if (fstat(fn, &buf) < 0){
		return -1;
	}
	return (buf.st_size);
}

/**
Returns file's mode
O -1 in case of error
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
	return (char *)(str+strlen(str)+2);
}

/**
Prints an error when an invalid option is used on the command line.
Will also call help() to show correct options
*/
void invalidOption(char *option) {
	printf("Invalid option %s\n\n",option);
	help();
}

/**
Prints version information
*/
void version() {
	printf ("Weborf %s\n",VERSION);
	printf ("Copyright (C) 2007 LSD: LSD Sicilian Developers.\n");
	printf ("This is free software.  You may redistribute copies of it under the terms of\n");
	printf ("the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n");
	printf ("There is NO WARRANTY, to the extent permitted by law.\n\n");

	printf ("Written by Salvo 'LtWorf' Tomaselli and Salvo Rinaldi.\n");
	printf ("Synchronized queue by Prof. Giuseppe Pappalardo.\n");
	exit(0);
}

/**
Prints command line help
 */
void help() {

	printf("\tUsage: weborf [OPTIONS]\n");
	printf("\tStart the weborf webserver\n\n");

	printf("  -p, --port	followed by port number to listen\n");
	printf("  -i, --ip	followed by IP address to listen (dotted format)\n");
	printf("  -b, --basedir	followed by absolute path of basedir\n");
	printf("  -u            followed by a valid uid.\n");
	printf("                If started by root weborf will use this user to read files and execute scripts\n");
	printf("  -h, --help	display this help and exit\n");
	printf("  -v, --version	print program version\n\n");

	printf("Default IP address is %s\n",LOCIP);
	printf("Default port is %s\n",PORT);
	printf("Default base directory is %s\n\n",BASEDIR);
	
	printf("Report bugs to <ltworf@galileo.dmi.unict.it>\n");
	exit(0);
}

/**
Searching for easter eggs within the code isn't fair!
*/
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

/**
This functions set enviromental variables according to the data present in the HTTP request
*/
void setEnvVars(char * http_param,char * method) { //Sets Enviroment vars	
	char * lasts;
	char * param;
	char * post_v=NULL;
	int i;
	int p_len;
	
	bool post=(strstr("POST",method)!=NULL);
	
	if (post) {//If the request is a POST, splits HTTP vars from POST vars
		//printf("Method: post\n");
		char *pointer;
		
		//+1 is due to the fact that some browsers put a \r\n\r\n at the beginning
		pointer=strstr (http_param+1,"\r\n\r\n");
		
		//If no post data is contained turns the request into a get
		if (pointer==NULL) {
			post=false;
		} else {
			pointer[0]='\0';
			post_v=pointer+4;
		}
	}
		
	//Removes the 1st part with the protocol
	param=strtok_r(http_param,"\r\n",&lasts);
	setenv("PROTOCOL",param,true);//Sets the protocol used
			
	//Cycles parameters
	while ((param=strtok_r(NULL,"\r\n",&lasts))!=NULL) {
				
		p_len=strlen(param);
		char * value=NULL;
				
		//Parses the parameter to split name from value
		for (i=0;i<p_len;i++) {
			if (param[i]==':' && param[i+1]==' ') {
				param[i]='\0';
				value=&param[i+2];
				break;
			}
		}
				
		strReplace(param,"-",'_');
		setenv(param,value,true);
	}
	
	if (post) {//Sets POST vars too
		
		param=strtok_r(post_v,"&",&lasts);
		putenv(param);//Sets the 1st token as var
		//Cycles vars
		while ((param=strtok_r(NULL,"&",&lasts))!=NULL) {
			putenv(param);
		}

		
	}
}
