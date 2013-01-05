#include <proto/exec.h>
#include <proto/dos.h>

#include <stdio.h>

#include "helloworld3.c"

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
	SUBZERO,
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
	setvbuf(stdout, NULL, _IOLBF, 0);
	int a=0, b=17;
	unsigned int c = 12345;
	char *charptr = "c";
	printf("charptr = 0x%x\n", &charptr);
	float hellofloat = 0.3;
	double dfloat = 0.0001;
	unsigned short iamshort = 3;
	void *q;
	struct hellostruct s = { 1234, 5 };
	printf("struct addr: 0x%x\n", &s);
	struct hellostruct *structptr = &s;
	enum helloenum e = TWO;

//	sleep (2);

	printf("Hello world!\n");

	a = hello(5);

	globalvariable = 4321;

	if(dfloat > 1.0)
	{
		printf("dfloat is a very large number\n");
		dfloat = 1.0;
	}
	
	helloworld3();

	for (; a < 15; a++)
	{
		printf("a = %d\n", a);
	}

	//printf("main: a = %d\n", a);

	printf("Waiting for signal...\n");
	//IExec->Wait(SIGBREAKF_CTRL_C);
	printf("Got it!\n");

	printf("Goodbye world!\n");
	
	return 0;
}
