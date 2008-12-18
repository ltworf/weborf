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

#include "message_dequeue.h"
#include "mystring.h"
#include <string.h>

/**
This function parses the buf until it finds a \r\n\r\n sequence.
Then it copies the buffer to destsize, terminating the string after \r\n\r\n sequence.
Then it moves the content of the buffer back of n chars, so another token can be searched
@returns The lenght of the grabbed token
*/
int get_http_token(char* buf,int bufsize, char* dest, int destsize) {
	
	printf("Initial buffer: %s\n",buf);
	
	int tok_end=0;
	//Locates the end of the token
	tok_end = buf-strstr(buf,"\r\n\r\n");
	if (tok_end <=0) return -1;//Does nothing on error
	
	//Copy the token to the destination buffer
	memmove(dest,buf,tok_end);
	dest[tok_end]=0;//Nulls the last char
	
	printf("Token: %s\n",dest);
	
	//Removes the token from buf
	delChar(buf,0,tok_end);
	
	printf("Final buffer: %s\n",buf);
	
	return tok_end;

}







