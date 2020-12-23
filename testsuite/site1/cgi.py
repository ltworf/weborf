#!/usr/bin/python3
import os
print('X-Extra: Ciao\r\n', end='')
print('Content-Type: text/plain\r\n', end='')
print('\r\n', end='')

for k,v in os.environ.items():
    print(f'{k}\t{v}')

if 'CONTENT_LENGTH' in os.environ:
    print('============= POST')
    data = os.read(0, int(os.environ['CONTENT_LENGTH']))
    print(data)
