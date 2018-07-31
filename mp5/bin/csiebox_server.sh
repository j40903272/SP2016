#!/bin/sh

# set bash fail at subcommand
#set -e

BOXDIR=$(cd "$(dirname $0)/.." && pwd)
SCRIPT="$BOXDIR/script/csiebox_server.sh"
DAEMON="$BOXDIR/bin/csiebox_server"
CONFIG="$BOXDIR/config/server.cfg"
RUNPATH=$(grep "run_path" $CONFIG | sed 's/.*=//g')
PIDFILE="$RUNPATH/csiebox_server.pid"

if [ -r /etc/default/locale ]; then
  . /etc/default/locale
  export LANG LANGUAGE
fi

if [ ! -d $RUNPATH ] || [ -z $RUNPATH ]; then
  echo "run_path is not a directory"
  exit 0
fi

if [ ! -x $DAEMON ]; then
  echo "cannot find excutable"
  exit 0
fi

START_ARGS="--pidfile $PIDFILE --make-pidfile --name $(basename $DAEMON) --startas $DAEMON -- $CONFIG -d"
STOP_ARGS="--pidfile $PIDFILE --name $(basename $DAEMON) --retry TERM/5/TERM/5"

case "$1" in
  start)
    echo "Starting CSIEBOX_SERVER"
    nohup $DAEMON $CONFIG -d &
    PID=$!
    RES=$?
    echo "csiebox is running. pid=$PID. result=$RES"
    echo $PID > $PIDFILE
    sleep 1
  ;;

  stop)
    if ! [ -f $PIDFILE ]; then
      echo "$PIDFILE not found"
      exit 1
    else
      PID=$(cat $PIDFILE)
      echo "Running process is $PID"
      kill -15 $PID
      RES=$?
      if [ $RES -eq 1 ]; then
        echo "kill $PID failed"
      elif [ $RES -eq 0 ]; then
        echo "kill $PID success"
        if [ -f $PIDFILE ]; then
          echo "remove pidfile $PIDFILE"
          rm $PIDFILE
        fi
      fi
    fi
  ;;

  *)
    echo "Usage: $0 {start|stop}"
    exit 1
  ;;
esac

exit 0
