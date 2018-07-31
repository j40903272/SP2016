#!/usr/bin/env bash

case "$1" in
help)
  echo "$0 [uninstall|help]"
  ;;
uninstall)
  rm -rf ~/.nvm
  sed -i 's/^.*nvm.*$//g' ~/.profile ~/.bashrc
  ;;
*)
  which wget
  if [ $? -ne 0 ]; then
    echo "can't find wget command"
    exit 1
  fi
  which git
  if [ $? -ne 0 ]; then
    echo "can't find git command"
    exit 1
  fi
  which curl
  if [ $? -ne 0 ]; then
    echo "can't find curl command"
    exit 1
  fi
  echo ". ~/.nvm/nvm.sh" >> ~/.bashrc
  wget -qO- https://raw.github.com/creationix/nvm/master/install.sh | sh
  . ~/.nvm/nvm.sh
  nvm install 0.11.0
  nvm use 0.11.0
  nvm alias default 0.11.0
  echo "please restart your terminal"
  ;;
esac
