init
reset halt

bp 0x08017540 2 hw
bp 0x08024114 2 hw
bp 0x08017514 2 hw

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

rbp all
resume
shutdown
