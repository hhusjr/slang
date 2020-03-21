`runtime/io.sl`

# DFS求全排列，并放在一个数组中
var bool vis[11];
var int result[1024][10], cnt, tmp[10];
func void dfs(int fill_pos, int tot) {
     if (fill_pos == tot) {
        var int i;
        for (i = 0; i < tot; i = i + 1) result[cnt][i] = tmp[i];
        cnt = cnt + 1;
        ret;
     }
     var int num;
     for (num = 1; num <= tot; num = num + 1) {
         if (vis[num]) continue;
         vis[num] = true;
         tmp[fill_pos] = num;
         dfs(fill_pos + 1, tot);
         vis[num] = false;
     }
}

var int n = 5, i, j;
dfs(0, n);
for (i = 0; i < cnt; i = i + 1) {
    for (j = 0; j < n; j = j + 1) {
        write(result[i][j]);
        write(' ');
    }
    writeln();
}
write("Total: ");
write(cnt);
writeln(" permutations.");
