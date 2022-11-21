rm -r output
mkdir output
make
scripts/runit.sh inputs/ output/ ./mmu
scripts/gradeit.sh refout/ output/

diff -b --speed-large-files refout//out11_16_f output//out11_16_f

