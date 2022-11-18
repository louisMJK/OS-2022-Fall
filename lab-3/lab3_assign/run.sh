rm -r output
mkdir output
make
scripts/runit.sh inputs/ output/ ./mmu
scripts/gradeit.sh refout/ output/

diff -b --speed-large-files refout//out1_16_r output//out1_16_r 
