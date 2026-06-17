#!/bin/bash

#############################################################################
# This script will install any python dependencies that will be needed
# by any *64-bit* Pythia code.
#
# To install the dependencies for a plugin, simply drag a requirements.txt
# file onto install_requirements64.sh
#############################################################################

# Pythia uses the system Python. Prefer the exact version, falling back to python3.
interpreter=python{version_dotted}
command -v "$interpreter" >/dev/null 2>&1 || interpreter=python3

echo ===============================================================================
echo Installing requirements using "$interpreter" from "$1"...
echo ===============================================================================

"${interpreter}" -m pip install  --upgrade --no-warn-script-location -r "$1"

if [ $? -ne 0 ]; then
    echo !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    echo An error happened during requirements installation. Your python environment is
    echo now in an undefined state!
    echo Fix the issues and reinstall the requirements!
    echo !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    exit 1
fi
