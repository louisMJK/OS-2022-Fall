make
./runit.sh outputs/ ./iosched
./gradeit.sh refout/ outputs/

diff -b refout//out_9_j outputs//out_9_j
