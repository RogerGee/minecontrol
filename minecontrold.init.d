#!/bin/sh
# minecontrold - DEBIAN startup script for minecontrol daemon
#  install as: /etc/init.d/minecontrold
#

### BEGIN INIT INFO
# Provides:             minecontrold
# Required-Start:       $remote_fs
# Required-Stop:        $remote_fs
# Default-Start:        2 3 4 5
# Default-Stop:         0 1 6
# Short-Description:    Minecontrol server
### END INIT INFO

start_minecontrold()
{
    minecontrold
}

stop_minecontrold()
{
    killall minecontrold
}

case "$1" in
    start)
	start_minecontrold
	;;
    stop)
	stop_minecontrold
	;;
    restart)
	stop_minecontrold
        sleep 2
	start_minecontrold
	;;
esac

exit 0