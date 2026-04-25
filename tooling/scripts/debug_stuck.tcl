init
reset halt

bp 0x08023dc8 2 hw
resume
wait_halt 5000

echo "=== kernel_thread hit ==="
rbp all
resume

after 3000

halt
echo "=== Where stuck ==="
set pc1 [reg pc]
echo "PC=$pc1"
set lr1 [reg lr]
echo "LR=$lr1"

set msp [mrw 0x2001ffe0]
echo "stack[0]=$msp"

resume
shutdown
