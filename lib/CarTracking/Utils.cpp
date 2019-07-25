#include <CarTracking.h>
// Phương thức reset Board điều khiển Arduino UNO, sử dụng lệnh assembly để dịch thanh ghi về vị trí đầu tiên.
void resetBoardUno()
{
    asm volatile("jmp 0");
}
