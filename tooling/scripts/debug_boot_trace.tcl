init
reset halt

bp 0x08020e14 2 hw
bp 0x08017514 2 hw
bp 0x08017884 2 hw
bp 0x08017954 2 hw

resume
wait_halt 5000
echo "=== BP1 ==="
set pc1 [reg pc]
echo "Hit: $pc1"

resume
wait_halt 5000
echo "=== BP2 ==="
set pc2 [reg pc]
echo "Hit: $pc2"

resume
wait_halt 5000
echo "=== BP3 ==="
set pc3 [reg pc]
echo "Hit: $pc3"

resume
wait_halt 5000
echo "=== BP4 ==="
set pc4 [reg pc]
echo "Hit: $pc4"

rbp all
resume
shutdown
