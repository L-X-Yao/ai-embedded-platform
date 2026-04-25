init
reset halt

bp 0x08020e5c 2 hw
resume
wait_halt 5000

echo "=== At main() entry ==="
set vtor [mrw 0xE000ED08]
echo "VTOR=$vtor"

set pc_val [reg pc]
echo "PC=$pc_val"

rbp all

bp 0x0802114e 2 hw
resume
wait_halt 5000

echo "=== At vsf_enable_interrupt ==="
set vtor2 [mrw 0xE000ED08]
echo "VTOR=$vtor2"

set systick_ctrl [mrw 0xE000E010]
echo "SysTick_CTRL=$systick_ctrl"

set icsr [mrw 0xE000ED04]
echo "ICSR=$icsr"

rbp all
resume

shutdown
