#include <proto/dos.h>
int main()
{
	int i;
	for(i=0; i < 2; i++)
	{
		i = 0;
		IDOS->Delay(1);
	}
	return 0;
}
