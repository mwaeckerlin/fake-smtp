#!/bin/bash

DIR=${1:-/var/mail}
PORT=${2:-25}
FIFO=${3:-/var/run/mail.fifo}

function log() {
    echo "$(date +"%Y-%m-%d %H:%M:%S") $*" >> "$DIR"/log
}
function send() {
    log "< $*"
    echo "$*"
}
function receive() {
    local __resultvar=$1
    local __result
    if read __result; then
        log "> $__result"
        eval $__resultvar="'${__result::-1}'"
        return 0
    else
        log "EOF"
        return 1
    fi
}
function store_mail() {
    local mailname=$1
    shift
    log "[$mailname] > $*"
    echo "$*" >> "$DIR"/"$mailname"
}
function smtp_server() {
    echo "220 Welcome to Fake SMTP Server - all mails stored in $DIR"
    unset helo from to mailname
    while receive line; do
        case "$line" in
            ("EHLO "*) send "250 OK"; helo=${line#EHLO };;
            ("HELO "*) send "250 OK"; helo=${line#HELO };;
            (QUIT) send "221 closing channel"; break;;
            ("MAIL FROM:"*) send "250 OK"; from=${line#MAIL FROM:};;
            ("RCPT TO:"*) send "250 OK"; to=${line#RCPT TO:};;
            (DATA) send "354 start mail input"
                mailname="$(date +"%Y%m%d%H%M%S")-${from}-${to}.txt"
                touch "$DIR"/"$mailname"
                while receive data; do
                    case "$data" in
                        (..) store_mail "${mailname}" ".";;
                        (.) send "250 OK"; break;;
                        (*) store_mail "${mailname}" "$data";;
                    esac
                done
                ;;
            (*) send "500 ERROR, unknown command: $line"
        esac
    done
}

test -d "$DIR" || mkdir "$DIR"
rm -f ${FIFO}
mkfifo ${FIFO}
while true; do
    log "**** opening port $PORT"
    smtp_server < ${FIFO} | netcat -l $PORT > ${FIFO}
done
