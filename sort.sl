func int partition(int a[], int low, int high) {
     var int i = low, j;
     var int x = a[i];
     var int tmp;
     for (j = low + 1; j <= high; j = j + 1) {
         if (a[j] <= x) {
            i = i + 1;
            tmp = a[i]; a[i] = a[j]; a[j] = tmp;
         }
     }
     tmp = a[i]; a[i] = a[low]; a[low] = tmp;
     ret i;
}

func void qsort(int a[], int low, int high) {
     if (low >= high) ret;
     var int i = partition(a, low, high);
     qsort(a, low, i - 1);
     qsort(a, i + 1, high);
}

func void qsort(int a[]) {
     qsort(a, 0, sizeof(a) - 1);
}

var int a[] = [4, 5, 3, 2, 5, 5, 4, 3, 2, 1, 2, 3, 4, 5], i;
qsort(a);
for (i = 0; i < sizeof(a); i = i + 1) printk a[i];
