# IO库 - Slang Runtime Library
# @author Junru Shen

# 输入字符
func char getch() {
    __svm__ GETCH;
    __svm__ RET;
}

# 输入字符串
func void read_str(char target[]) {
    var int buffer_size = sizeof(target);
    var int cur = 0;
    var char ch = '\0';
    while (ch != '\n' && ch != ' ') {
        target[cur] = ch = getch();
        if (cur < buffer_size - 2) cur = cur + 1;
        else break;
    }
    target[cur + 1] = '\0';
}

# 输出字符
func void write(char ch) {
    __svm__ LOAD_NAME &ch;
    __svm__ PUTCH;
}

# 输出字符串
func void write(char ch[]) {
    var int i;
    for (i = 0; ch[i] != '\0'; i = i + 1) write(ch[i]);
}

var char __NUMBER_DIGITS[] = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9'];
# 输出整型数字
func void write(int n) {
     if (n == 0) {
        write('0');
        ret;
     }
    if (n < 0) {
        write('-');
        n = -n;
    }
    var char s[20];
    var int t, i;
    for (1; n > 0; n = n / 10) {
        s[t] = __NUMBER_DIGITS[n % 10];
        t = t + 1;
    }
    for (i = t - 1; i >= 0; i = i - 1) write(s[i]);
}

# 输出一个空行
func void writeln() {
    write('\n');
}

# 输出一行，仅有一个字符
func void writeln(char ch) {
    write(ch);
    write('\n');
}

# 输出一行字符串
func void writeln(char ch[]) {
    write(ch);
    write('\n');
}

# 输出一行数字
func void writeln(int n) {
    write(n);
    write('\n');
}
