init
reset halt

resume
after 5000
halt

echo "=== MCU State after 5s ==="
set pc_val [reg pc]
echo "PC=$pc_val"

set cfsr [mrw 0xE000ED28]
set hfsr [mrw 0xE000ED2C]
echo "CFSR=$cfsr HFSR=$hfsr"

resume
shutdown
