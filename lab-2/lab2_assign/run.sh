rm -r output
mkdir output
make
./runit.sh output/ ./sched
./gradeit.sh refout/ output/
