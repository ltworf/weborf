User friendly webserver

qweborf
=======
Provides a GUI to share local files.

It can do NAT traversal to share files outside of the local network.

Can enable authentication and sending directories as `.tar.gz` files.


weborf
======
The web server used by qweborf.

Can be used from inetd, supports WebDAV, caching, CGI, virtual hosts.


Compile
=======

These are the steps to compile weborf.

```
autoreconf -f -i # Only if you cloned from git

make clean
./configure
make
sudo make install
```

To compile qweborf.

```
pyuic5 qweborf/main.ui > qweborf/main.py

python3 qweborf.setup.py install
install -D qweborf/qweborf /usr/local/bin/qweborf
```
