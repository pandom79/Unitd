#!/bin/sh

[ "${DESTDIR: -1}" == '/' ] && DESTDIR=${DESTDIR::-1}
PREFIX="${DESTDIR}$1"
SBIN_PATH="$PREFIX/$2"
UNITS_PATH="${DESTDIR}$3"
UNITS_USER_PATH="${DESTDIR}$4"
UNITS_ENAB_PATH="${DESTDIR}$5"
UNITD_TIMER_DATA_PATH="${DESTDIR}$6"

# Export like environment variables to satisfy unitd check unit script.
export UNITS_PATH="$UNITS_PATH"
export UNITS_USER_PATH="$UNITS_USER_PATH"
export UNITS_ENAB_PATH="$UNITS_ENAB_PATH"
export UNITD_TIMER_DATA_PATH="$UNITD_TIMER_DATA_PATH"

printf "Received the following parameter:\n"
printf "DESTDIR=$DESTDIR\n"
printf "PREFIX=$PREFIX\n"
printf "SBIN_PATH=$SBIN_PATH\n"
printf "UNITS_PATH=$UNITS_PATH\n"
printf "UNITS_USER_PATH=$UNITS_USER_PATH\n"
printf "UNITS_ENAB_PATH=$UNITS_ENAB_PATH\n"
printf "UNITD_TIMER_DATA_PATH=$UNITD_TIMER_DATA_PATH \n"

printf "=> Creating symbolic link for zzz ...\n"
ln -sfv "$SBIN_PATH/zzz" "$SBIN_PATH/ZZZ"

printf "=> Executing unitd check unit ...\n"
../src/extra/init.state/scripts/unitd-check.sh

printf "=> Copying units ...\n"
cp -v ../units/*.unit "$UNITS_PATH"

printf "=> Enabling units ...\n"
STATES="custom.state graphical.state multi-user-net.state multi-user.state single-user.state"
for state in ${STATES[@]}; do
    [ "$state" != "single-user.state" ] && \
    ln -sfv "$UNITS_PATH/agetty-1.unit" "$UNITS_ENAB_PATH/$state" || \
    ln -sfv "$UNITS_PATH/sulogin.unit" "$UNITS_ENAB_PATH/$state"
done

printf "Done!\n"
