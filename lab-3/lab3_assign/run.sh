rm -r output
mkdir output
make
scripts/runit.sh inputs/ output/ ./mmu
scripts/gradeit.sh refout/ output/

# diff -b --speed-large-files refout//out8_16_f output//out8_16_f

