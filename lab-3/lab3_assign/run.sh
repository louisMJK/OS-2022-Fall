rm -r output
mkdir output
make
scripts/runit.sh inputs/ output/ ./mmu
scripts/gradeit.sh refout/ output/

diff -b --speed-large-files refout//out1_32_f output//out1_32_f

