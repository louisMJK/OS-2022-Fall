rm -r output
mkdir output
make
scripts/runit.sh inputs/ output/ ./mmu
scripts/gradeit.sh refout/ output/

diff -b --speed-large-files refout//out4_16_f output//out4_16_f