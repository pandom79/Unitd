#!/bin/sh -e

export PATH=$PATH

. $UNITD_CONF_PATH/unitd.conf

[ -n "$VIRTUALIZATION" ] && exit 0

TTYS=${TTYS:-12}
if [ -n "$FONT" ]; then
    _index=0
    while [ ${_index} -le $TTYS ]; do
        setfont ${FONT_MAP:+-m $FONT_MAP} ${FONT_UNIMAP:+-u $FONT_UNIMAP} \
            $FONT -C "/dev/tty${_index}"
        _index=$((_index + 1))
    done
fi

if [ -n "$KEYMAP" ]; then
    loadkeys -q -u ${KEYMAP}
fi

if [ -n "$HARDWARECLOCK" ]; then
    TZ=$TIMEZONE hwclock --systz \
        ${HARDWARECLOCK:+--$(echo $HARDWARECLOCK | tr A-Z a-z) --noadjfile}
fi
