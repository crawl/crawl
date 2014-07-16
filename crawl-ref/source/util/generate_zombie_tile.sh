#!/bin/bash

# This script is a quick and dirty way to generate sometimes plausible
# zombie tiles. If a human drawn tile is available, use it. Otherwise,
# the output of this script may be preferable.
# It relies on imagemagick.

img=${1%.png}
convert ${img}.png -colorspace gray tmp_${img}_gray.png
convert -blur 2x3 tmp_${img}_gray.png -background none tmp_${img}_blur.png
composite ${img}.png tmp_${img}_blur.png -compose difference tmp_${img}_diff.png
convert -auto-level tmp_${img}_diff.png tmp_${img}_normal.png
convert tmp_${img}_normal.png -level 75%,100% tmp_${img}_blowout.png
convert tmp_${img}_blowout.png -channel R -fx '(u.g+u.r+u.b)/3' -channel B -fx '(u.g+u.r+u.b)/3' -channel G -fx '0' tmp_${img}_highlight.png
convert tmp_${img}_highlight.png -auto-level tmp_${img}_highlight2.png
composite tmp_${img}_gray.png tmp_${img}_highlight2.png -compose screen ${img}_zombie.png
rm tmp_*.png
