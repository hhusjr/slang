# 数学计算库 - Slang Runtime Library
# @author Junru Shen

`runtime/conv.sl`

func int abs(int x) {
    if (x >= 0) ret x;
    ret -x;
}

func float abs(float x) {
    if (x >= 0) ret x;
    ret -x;
}

func int sqrt(int x) {
    # Fast bitwise_verification method designed for integers
    # Paper: Computing Integer Square Roots, James Ulery
    var int tmp = 0, v_bit = 15, n = 0, b = 32768;
    if (x <= 1) ret x;
    while (true) {
        tmp = ((n << 1) + b) << v_bit;
        v_bit = v_bit - 1;
        if (x >= tmp) {
            n = n + b;
            x = x - tmp;
        }
        b = b >> 1;
        if (b == 0) ret n;
    }
}

func float sqrt(float x) {
    # Newton's iteration method
    var float last = 0.0, res = 1.0;
    while (abs(res - last) > 0.0000001) {
        last = res;
        res = (res + x / res) / 2;
        # printk res;
    }
    ret res;
}
