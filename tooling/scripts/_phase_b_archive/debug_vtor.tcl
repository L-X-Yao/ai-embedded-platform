init
reset halt

set vtor [mrw 0xE000ED08]
echo "VTOR=$vtor"

set sp0 [mrw 0x08010000]
echo "VT_SP=$sp0"

set reset [mrw 0x08010004]
echo "VT_Reset=$reset"

set nmi [mrw 0x08010008]
echo "VT_NMI=$nmi"

set hf [mrw 0x0801000c]
echo "VT_HardFault=$hf"

set svc [mrw 0x0801002c]
echo "VT_SVC=$svc"

set pendsv [mrw 0x08010038]
echo "VT_PendSV=$pendsv"

set systick [mrw 0x0801003c]
echo "VT_SysTick=$systick"

resume
shutdown
