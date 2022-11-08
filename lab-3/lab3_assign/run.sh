rm -r output
mkdir output
make
scripts/runit.sh inputs/ output/ ./mmu
# ./gradeit.sh refout/ output/
