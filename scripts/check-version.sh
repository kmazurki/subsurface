#!/bin/sh

# $1	- version string
# options:
#	-c	colored grep (implies -d)
#	-d	debug
#	-r	release (exit status error; when called from Makefile)

# To validate relevant files are up to date, you would run the script
# from command line before tagging:
#
#	$ scripts/check-version.sh -cr <tag>

set -eu
#set -x

# You would typically add/remove files to/from the list
files="Documentation/user-manual.txt Makefile README.md ReleaseNotes/ReleaseNotes.txt"

whine() {
	echo "$0: $*" >&2
}

croak() {
	whine "$*"
	exit 1
}

color=n
debug=n
release=n
while getopts cdr opt; do
	case $opt in
		c)
			color=y
			debug=y
			;;
		d)
			debug=y
			;;
		r)
			release=y
			;;
		*)
			croak "invalid option"
			;;
	esac
done
shift $(($OPTIND - 1))

if [ $debug = y ]; then
	opts=-n
else
	opts=-q
fi
[ $color = n ] || opts="${opts:+$opts }--color"

v=${1:-}
v=${v#v}
case $v in
	*-*)
		# Ignore development versions
		if [ $release = y ]; then
			croak "'$v' not a release tag"
		else
			exit 0
		fi
		;;
	''|*[!.0-9]*)
		croak "invalid version string '$v'"
		;;
esac
whine "checking for version $v"

saveIFS=$IFS
IFS=.
set -- $v
IFS=$saveIFS
v=
while [ $# -gt 0 ]; do
	v=${v:+$v\\.}$1
	shift
done

sts=0
for f in $files; do
	grep	-EH $opts \
		-e "(VERSION=|[Ss]ubsurface[[:blank:]]+)?\<v?$v[.0-9]*\>" \
		$f || {
		[ $release != y ] || sts=1
		whine "'$f' may need updating"
	}
done

exit $sts
