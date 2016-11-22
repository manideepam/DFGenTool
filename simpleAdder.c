#include<stdio.h>

int main() {
	int data[1000];
	int i, n, sum=0;
	printf("Enter the value of n \n");
	scanf("%d",&n);

	for(i=0; i<n; i++) {
		if(i%2== 0)
			data[i] = i;
		else
			data[i] = 2*i;
		sum = sum + data[i];
	}

	printf("sum is = %d\n", sum);
}
