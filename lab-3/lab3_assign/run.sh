rm -r output
mkdir output
make
scripts/runit.sh inputs/ output/ ./mmu
scripts/gradeit.sh refout/ output/

