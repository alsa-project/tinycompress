#!/bin/sh

autoreconf --verbose --force --install || {
	echo 'autogen.sh failed';
	exit 1;
}
