cd vm
make clean
make
cd build
source ../../activate
# pintos-mkdisk filesys.dsk 10
pintos --gdb -v -k -m 20   --fs-disk=10 -p tests/userprog/args-single:args-single --swap-disk=4 -- -q   -f run 'args-single onearg'
# pintos -v -k -m 20   --fs-disk=10 -p tests/vm/pt-grow-stack:pt-grow-stack --swap-disk=4 -- -q   -f run pt-grow-stack
# pintos -v -k -m 20   --fs-disk=10 -p tests/vm/pt-grow-bad:pt-grow-bad --swap-disk=4 -- -q   -f run pt-grow-bad
