// #include <stdio.h>
// #include <stdlib.h>
// #include <stdint.h>

// int rdrand16_step(uint16_t *rand)
// {
// 	unsigned char ok;

// 	/* rdrand dx */
// 	__asm__ __volatile__(".byte 0x0f,0xc7,0xf0; setc %1"
// 		: "=a" (*rand), "=qm" (ok)
// 			:
// 			: "dx"
// 			);
    

// 	return ok;
// }

// int main(int argc, char *argv[]){
//     unsigned char ok;
//     uint16_t *rand = (uint16_t*)malloc(sizeof(uint16_t));
//     int n;
//     while(1){
//         __asm__ __volatile__ (
//             "db 048h, 00Fh, 0C7h, 0F0h"
//             // ".wrod 0x0f,0xc7,0xf0; setc %0"
//             :"=a"(n)
//             :
//         );
//     printf("%d\n", n);

// 	/* rdrand dx */
// 	// __asm__ __volatile__
// 	// (
// 	// 	".byte 0x0f,0xc7,0xf0; setc %1"
// 	// 	: "=a" (*rand), "=qm" (ok)
// 	// 	:
// 	// 	: "dx"
//     // );
//     // printf("%c\n", ok + 48);
//         // printf("%c\n", rdrand16_step(&rand));
//     }
//     return 0;
// }

#include <stdio.h>
#include <stdint.h>

uint64_t random_uint64(void) {
    uint64_t result;
    unsigned char ok;
    __asm__ __volatile__("rdrand %0; setc %1"
                         : "=r" (result), "=qm" (ok));
    if (ok) {
        return result;
    } else {
        // RDRAND failed, fall back to something else
        return 0;
    }
}

int main(void) {
    uint64_t random_number = random_uint64() % 100;
    printf("Random number: %llu\n", random_number);
    return 0;
}
