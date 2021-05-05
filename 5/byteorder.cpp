#include <iostream>

void byteorder() {
    union {
        short value;
        char union_bytes[sizeof(short)];
    } test;
    test.value = 0x0102;
    printf("%d\n", test.value);
    if ((test.union_bytes[0] == 1) && test.union_bytes[1] == 2) {
        printf("big endian\n");
    } else if ((test.union_bytes[0] == 2) && (test.union_bytes[1] == 1)) {
        printf("little endian\n");
    } else {
        printf("unnknow...\n");
    }
}

int main() {
    std::cout << sizeof(short) << std::endl;
    printf("%d\n", 0x0102);
    byteorder();
    return 0;
}
