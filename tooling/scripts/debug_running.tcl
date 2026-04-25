init
halt

echo "=== MCU State ==="
set pc_val [reg pc]
echo "PC=$pc_val"

set vtor [mrw 0xE000ED08]
echo "VTOR=$vtor"

set systick_ctrl [mrw 0xE000E010]
set systick_load [mrw 0xE000E014]
set systick_val [mrw 0xE000E018]
echo "SysTick: CTRL=$systick_ctrl LOAD=$systick_load VAL=$systick_val"

set icsr [mrw 0xE000ED04]
echo "ICSR=$icsr"

set primask [reg primask]
echo "PRIMASK=$primask"

resume
shutdown
