#!/bin/bash

# Exit script on any failure
set -e

# Remove any previous 'latex' directory
rm -rf latex
# Generate documentation in LaTeX source using `doxygen`
doxygen
# Prepare the LaTeX source by removing weird HTML tags
cd latex
sed -r -i 's|\$<\$/?tt\$>\$(\\\+)?||ig' *.tex
# Compile the LaTeX source into a PDF and copy it to the parent directory
make
cp refman.pdf ..
