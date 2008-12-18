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

#define VERSION "0.4"

//Sistema
#define ROOTUID 0

//Rete
//Indirizzo su cui si accettano connessioni
#define LOCIP "0.0.0.0"
#define MAXQ 512
//Porta
#define PORT "8080"

//Threads
#define MAXTHREAD 200
#define INITIALTHREAD 12
#define LOWTHREAD 3
#define MAXFREETHREAD 12
#define THREADCONTROL 300


//Server
#define INDEX "index.html"
#define BASEDIR "/var/www"

//instance
#define INBUFFER 900
#define FILEBUF 50000
#define MAXSCRIPTOUT 1024000
#define HEADBUF 100

//Errors
#define ERR404 "<html><head><title>Pagina non trovata</title></head><body><h1>Errore 404</h1><p>Pagina non trovata<p></body></html>"

//HTML
#define HTMLHEAD "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"><html><head><title>Weborf</title></head><body>"

#define HTMLFOOT "</body></html>"
