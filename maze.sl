`runtime/io.sl`

# 宽度优先搜索
var char map[30][30] = [
    "...***....*...***....",
    "**.*...**...*.*.*..**",
    ".*...*****...*..*****",
    "****.....***..*...***",
    "...***...**.....*.**.",
    "...***...**...***...."
];
# 入口在左上角，出口在左下角
# 走出上面这个6行21列的地图，打印最短路线
var int n = 6;
var int m = 21;

# 可以往四个方向走
var int dx[] = [-1, 1, 0, 0];
var int dy[] = [0, 0, -1, 1];

# 演示一下前向声明特性
func void print_solution(int cur, int q_x[], int q_y[], int q_dir[], int q_fa[], int n, int m);
func void bfs(int n, int m);
func void print_dir_text(int dir);

# 主程序
bfs(n, m);

# 宽搜
func void bfs(int n, int m) {
     # 目前尚未支持结构体，考虑拆成四个队列
     var int q_x[3000], # 行x队列
             q_y[3000], # 列x队列
             q_fa[3000], # 父节点队列
             q_dir[3000]; # 行走方向队列
     var bool vis[30][30]; # 记录节点是否被访问过
     var int front, rear;

     front = rear = 0;
     q_x[rear] = q_y[rear] = 0;
     q_dir[rear] = q_fa[rear] = -1;
     rear = rear + 1;

     while (front <= rear) {
           if (q_x[front] == n - 1 && q_y[front] == m - 1) {
              writeln("One of the best route: ");
              writeln("Now you are at (0, 0), let's begin!");
              # 使用递归方式输出最优解
              print_solution(front, q_x, q_y, q_dir, q_fa, n, m);
              writeln("Congradulations!");
              ret;
           }
           var int i;
           for (i = 0; i < 4; i = i + 1) {
               var int tx = q_x[front] + dx[i];
               var int ty = q_y[front] + dy[i];
               if (tx < 0 || tx >= n || ty < 0 || ty >= m) continue;
               # 暂时不支持短路
               if (vis[tx][ty] || map[tx][ty] != '.') continue;
               vis[tx][ty] = true;
               q_x[rear] = tx; q_y[rear] = ty;
               q_dir[rear] = i; q_fa[rear] = front;
               rear = rear + 1;
           }
           front = front + 1;
     }

     writeln("Not found.");
}

# 获取方向对应的文本
func void print_dir_text(int dir) {
     var char text[20];
     if (dir == 0) text = "GO UP";
     else if (dir == 1) text = "GO DOWN";
     else if (dir == 2) text = "GO LEFT";
     else text = "GO RIGHT";
     write(text);
}

# 递归输出最优解
func void print_solution(int cur, int q_x[], int q_y[], int q_dir[], int q_fa[], int n, int m) {
     if (q_fa[cur] == -1) ret;
     print_solution(q_fa[cur], q_x, q_y, q_dir, q_fa, n, m);
     write("Then, ");
     print_dir_text(q_dir[cur]);
     write(", and you will arrive at (");
     write(q_x[cur]);
     write(", ");
     write(q_y[cur]);
     writeln(")");
     var int i;
     for (i = 0; i < n; i = i + 1) {
         var int j;
         for (j = 0; j < m; j = j + 1) {
             if (q_x[cur] == i && q_y[cur] == j) write('^');
             else write(map[i][j]);
         }
         writeln();
     }
     getch();
}