init
halt

echo "=== FAULT REGISTERS ==="
set cfsr [mrw 0xE000ED28]
set hfsr [mrw 0xE000ED2C]
set mmfar [mrw 0xE000ED34]
set bfar [mrw 0xE000ED38]
echo "CFSR=$cfsr HFSR=$hfsr MMFAR=$mmfar BFAR=$bfar"

echo "=== EXCEPTION FRAME at MSP 0x2001ff58 ==="
set r0  [mrw 0x2001ff58]
set r1  [mrw 0x2001ff5c]
set r2  [mrw 0x2001ff60]
set r3  [mrw 0x2001ff64]
set r12 [mrw 0x2001ff68]
set lr  [mrw 0x2001ff6c]
set pc  [mrw 0x2001ff70]
set xpsr [mrw 0x2001ff74]
echo "R0=$r0 R1=$r1 R2=$r2 R3=$r3"
echo "R12=$r12 LR=$lr PC=$pc xPSR=$xpsr"

resume
shutdown
