//#include <proto/dos.h>

//#include <stdio.h>


int hello(int a)
{
	int hello;

	a=a+3;
	//asm ("	trap");

	//IDOS->Printf("hello: hello hello :-)\n");
	return a;
}


int main()
{
	int a, b=17;

//	sleep (2);

	printf("Hello world!\n");

	a = hello(5);

	for (; a < 15; a++)
	{
		printf("a = %d\n", a);
	}

	//printf("main: a = %d\n", a);

	printf("Goodbye world!\n");
	
	return 0;
}
