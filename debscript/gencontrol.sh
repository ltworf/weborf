#!/bin/bash
echo "Source: weborf"
echo "Binary: weborf"
echo "Architecture: any"
echo "Version: "`./weborf -v | fgrep Weborf | cut -d' ' -f2`
echo "Maintainer: Salvo 'LtWorf' Tomaselli <tiposchi@tiscali.it>"
echo "Homepage: http://galileo.dmi.unict.it/wiki/weborf/"
echo "Standards-Version: 3.8.3"
echo "Build-Depends: build-essential (>= 11.4)"
echo ""
        echo "Package: weborf"
        echo "Version: "`./weborf -v | fgrep Weborf | cut -d' ' -f2`
        echo "Architecture: "`apt-cache show bash | fgrep Architecture | cut -d' ' -f2 | uniq`
        echo "Maintainer: Salvo 'LtWorf' Tomaselli <tiposchi@tiscali.it>"
        echo "Installed-Size: "`du -s --apparent-size weborf | cut -f1`
        echo "Homepage: http://galileo.dmi.unict.it/wiki/weborf/"
        echo "Priority: optional"
        echo "Suggests: php5-cgi (>= 5.2.10.dfsg.1-2.2)"
        echo "Depends: bash (>=3.2-4)"
        echo "Section: web"
        echo "Description: Fast and small webserver to be run in user-mode"
        echo " Weborf is a webeserver mainly meant to be executed without root privileges."
	echo " Using it, users can fastly share directories over the web. Without having to"
	echo " deal with configuration or without being forced to share the same directory"
	echo " all the time."
	echo " It also supports php5-cgi."
