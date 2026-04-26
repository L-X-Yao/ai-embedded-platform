init
reset halt

bp 0x08023dc8 2 hw
bp 0x08023dae 2 hw
bp 0x08017514 2 hw

resume
wait_halt 8000
echo "=== BP1 ==="
set pc1 [reg pc]
echo "PC=$pc1"

resume
wait_halt 5000
echo "=== BP2 ==="
set pc2 [reg pc]
echo "PC=$pc2"

resume
wait_halt 5000
echo "=== BP3 ==="
set pc3 [reg pc]
echo "PC=$pc3"

rbp all
resume
shutdown
