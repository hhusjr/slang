`runtime/io.sl`
`runtime/conv.sl`
`runtime/math.sl`

# Algorithm: https://www.zhihu.com/question/20187195

func void heart1() {
     var float x, y;
     for (y = 1.5; y > -1.5; y = y - 0.1) {
         for (x = -1.5; x < 1.5; x = x + 0.05) {
             var float a;
             a = x * x + y * y - 1;
             if (a * a * a - x * x * y * y * y <= 0.0) write('*');
             else write(' ');
         }
         writeln();
     }
}

func void heart2() {
     var char use[] = ".:-=+*#%@";
     var float x, y;
     for (y = 1.5; y > -1.5; y = y - 0.1) {
         for (x = -1.5; x < 1.5; x = x + 0.05) {
             var float a, f;
             a = x * x + y * y - 1;
             f = a * a * a - x * x * y * y * y;
             if (f <= 0.0) write(use[to_int(f * -8.0)]);
             else write(' ');
         }
         writeln();
     }
}

func float f(float x, float y, float z) {
     var float a = x * x + 9.0 / 4.0 * y * y + z * z - 1;
     ret a * a * a - x * x * z * z * z - 9.0 / 80.0 * y * y * z * z * z;
}

func float h(float x, float z) {
     var float y;
     for (y = 1.0; y >= 0.0; y = y - 0.001)
         if (f(x, y, z) <= 0.0) ret y;
     ret 0.0;
}

func void heart3() {
     var float x, z;
     var char use[] = ".:-=+*#%@";
     for (z = 1.5; z > -1.5; z = z - 0.05) {
         for (x = -1.5; x < 1.5; x = x + 0.025) {
             var float v = f(x, 0.0, z);
             if (v <= 0.0) {
                var float y0 = h(x, z),
                          ny = 0.01,
                          nx = h(x + ny, z) - y0,
                          nz = h(x, z + ny) - y0,
                          nd = 1.0 / sqrt(nx * nx + ny * ny + nz * nz),
                          d  = (nx + ny - nz) * nd * 0.5 + 0.5;
               write(use[to_int(d * 5.0)]);
             } else write(' ');
         }
         writeln();
     }
}

writeln("This is a heart full of love.");
heart1();
getch();

writeln("But that's not enough...");
heart2();
getch();

writeln("Indeed...");
heart3();
getch();

writeln("Over.");
