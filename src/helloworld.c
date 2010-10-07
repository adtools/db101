//#include <proto/dos.h>

//#include <stdio.h>

typedef unsigned int mytype;
//typedef char *ptrtype;

int globalvariable = 1234;

void hello2()
{
	printf("halihello\n");
}

int hello(int a)
{
	int hello;

	a=a+3;
	//asm ("	trap");


	hello2();
	//IDOS->Printf("hello: hello hello :-)\n");
	return a;
}


int main()
{
	int a, b=17;
	char *charptr;
	float hellofloat = 0.3;

//	sleep (2);

	printf("Hello world!\n");

	a = hello(5);

	globalvariable = 4321;

	for (; a < 15; a++)
	{
		printf("a = %d\n", a);
	}

	//printf("main: a = %d\n", a);

	printf("Goodbye world!\n");
	
	return 0;
}
