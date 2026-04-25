init
reset halt

bp 0x0802114e 2 hw
resume
wait_halt 5000

echo "=== At vsf_enable_interrupt ==="
echo "NVIC Interrupt Set-Pending Registers:"
set ispr0 [mrw 0xE000E200]
set ispr1 [mrw 0xE000E204]
set ispr2 [mrw 0xE000E208]
echo "ISPR0=$ispr0 ISPR1=$ispr1 ISPR2=$ispr2"

echo "NVIC Interrupt Set-Enable Registers:"
set iser0 [mrw 0xE000E100]
set iser1 [mrw 0xE000E104]
set iser2 [mrw 0xE000E108]
echo "ISER0=$iser0 ISER1=$iser1 ISER2=$iser2"

set icsr [mrw 0xE000ED04]
echo "ICSR=$icsr"

set shcsr [mrw 0xE000ED24]
echo "SHCSR=$shcsr"

set systick [mrw 0xE000E010]
echo "SysTick_CTRL=$systick"

rbp all

bp 0x0802115a 2 hw
resume
wait_halt 3000

echo "=== At evtq loop entry ==="
set pc_val [reg pc]
echo "PC=$pc_val"

set icsr2 [mrw 0xE000ED04]
echo "ICSR=$icsr2"

rbp all
resume

shutdown
