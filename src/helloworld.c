//#include <proto/dos.h>

#include <stdio.h>

//#include "helloworld3.c"

typedef unsigned int mytype;
//typedef char *ptrtype;

int globalvariable = 1234;

extern void helloworld_external();

struct hellostruct
{
	int a;
	short b;
};

enum helloenum
{
	ZERO,
	ONE,
	TWO,
	INFINIT
};

void hello2()
{
	printf("halihello\n");
	helloworld_external();
	//helloworld3();
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
	char *charptr = "\n";
	float hellofloat = 0.3;
	double dfloat = 0.0001;
	unsigned short iamshort = 3;
	void *q;
	struct hellostruct s = { 1234, 5 };
	struct hellostruct *structptr = &s;
	enum helloenum e = TWO;
	register int regint;

	for(regint=0; regint < 10; regint++)
		printf("regint = %d\n", regint);
	
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
