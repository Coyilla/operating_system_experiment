// cmd2.c 简单计算器
# include <stdio.h>
# include <stdlib.h>

int main(){
	printf("简单计算器：\n");
	printf("支持操作：+ - * / %% \n");
	printf("格式：数字1 运算符 数字2 \n");
	printf("例如：5 + 3\n");
	printf("输入'q' 退出\n\n");

	while(1){
		double num1,num2,result;
		char op;

		printf("请输入表达式：");
		if(scanf("%lf %c %lf",&num1,&op,&num2)!=3){
			break;
		}

		switch (op){
			case '+':
				result=num1+num2;
				printf("结果：%.2f\n",result);
				break;
			case '-':
				result=num1-num2;
                                printf("结果：%.2f\n",result);
                                break;
			case '*':
				result=num1*num2;
                                printf("结果：%.2f\n",result);
                                break;
			case '/':
				if (num2!=0){
					result=num1/num2;
					printf("结果：%.2f\n",result);
				}else{
					printf("错误：除数不能为0!\n");
				}
				break;
			case '%':
				if(num2!=0){
					result =(int)num1 % (int)num2;
					printf("结果：%.2f\n",result);
				}else{
					printf("错误：除数不能为0!\n");
				}
				break;
			default:
				printf("错误：不支持运算符%c\n",op);
		}
	}
	return 0;
}

