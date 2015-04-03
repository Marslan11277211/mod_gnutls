#!/bin/bash

set -e

if [ -z "${BACKEND_HOST}" ]; then
    export BACKEND_HOST="localhost"
fi
if [ -z "${BACKEND_IP}" ]; then
    export BACKEND_IP="::1"
fi
if [ -z "${BACKEND_PORT}" ]; then
    export BACKEND_PORT="9934"
fi
if [ -z "${BACKEND_LOCK}" ] && [ -n "${testdir}" ]; then
    BACKEND_LOCK="$(realpath ${testdir}/../../backend.lock)"
fi

function backend_apache
{
    dir="${1}"
    conf="${2}"
    action="${3}"
    lockfile="${4}"

    if [ -n "${lockfile}" ]; then
	flock_cmd="flock -w 10 ${lockfile}"
    fi

    TEST_NAME="$(basename "${dir}")"
    (
	export TEST_NAME
	export TEST_IP="${BACKEND_IP}"
	export TEST_PORT="${BACKEND_PORT}"
	case $action in
	    start)
		cd "${dir}"
		${flock_cmd} \
		    /usr/sbin/apache2 -f "$(pwd)/${conf}" -k start || return 1
		;;
	    stop)
		cd "${dir}"
		/usr/sbin/apache2 -f "$(pwd)/${conf}" -k stop || return 1
		;;
	esac
    )
}