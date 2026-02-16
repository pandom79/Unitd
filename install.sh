#!/bin/sh -e

WRKDIR="$PWD"
LEVELS=""
STATES="custom.state graphical.state multi-user-net.state multi-user.state single-user.state"
[ "${DESTDIR: -1}" == '/' ] && DESTDIR=${DESTDIR::-1}
PREFIX="${DESTDIR}$1"
SBIN_PATH="$PREFIX/$2"
UNITS_PATH="${DESTDIR}$3"
UNITS_USER_PATH="${DESTDIR}$4"
UNITS_ENAB_PATH="${DESTDIR}$5"
UNITD_TIMER_DATA_PATH="${DESTDIR}$6"
SYSCONFDIR="${7:1}"

# Set environment variables to satisfy unitd check unit script.
export UNITS_PATH="$UNITS_PATH"
export UNITS_USER_PATH="$UNITS_USER_PATH"
export UNITS_ENAB_PATH="$UNITS_ENAB_PATH"
export UNITD_TIMER_DATA_PATH="$UNITD_TIMER_DATA_PATH"

printf "Received the following parameters:\n"
printf "DESTDIR=$DESTDIR\n"
printf "PREFIX=$PREFIX\n"
printf "SBIN_PATH=$SBIN_PATH\n"
printf "UNITS_PATH=$UNITS_PATH\n"
printf "UNITS_USER_PATH=$UNITS_USER_PATH\n"
printf "UNITS_ENAB_PATH=$UNITS_ENAB_PATH\n"
printf "UNITD_TIMER_DATA_PATH=$UNITD_TIMER_DATA_PATH \n"

printf "=> Creating symbolic link for zzz ...\n"
cd "$SBIN_PATH"
ln -sfv zzz ZZZ

printf "=> Executing unitd check unit ...\n"
cd "$WRKDIR"
../src/extra/init.state/scripts/unitd-check.sh

printf "=> Copying units ...\n"
cp -v ../units/*.unit "$UNITS_PATH"

printf "=> Enabling units ...\n"
# We have to use relative symlinks to enable units.
# Let's figure out how many levels have to go up.
cd "$UNITS_PATH"
[ -z "$DESTDIR" ] && DESTDIR="/"
while [ $(pwd) != "$DESTDIR" ]; do
	cd ..
	LEVELS+="../"
done
# Re-enter in UNITS_PATH to set relative symlinks.
cd "$UNITS_PATH"
for state in ${STATES[@]}; do
	[ "$state" != "single-user.state" ] &&
		ln -rsfv agetty-1.unit "${LEVELS}${SYSCONFDIR}/unitd/units/$state" ||
		ln -rsfv sulogin.unit "${LEVELS}${SYSCONFDIR}/unitd/units/$state"
done

printf "Done!\n"
