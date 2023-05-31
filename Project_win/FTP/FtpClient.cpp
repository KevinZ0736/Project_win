#include"_ftp.h"
#include"_public.h"

Cftp ftp;

int  main()
{
	if (ftp.login("124.222.175.81:21", "KevinZ", "19980408") == false)
	{
		printf("ftp.login(124.222.175.81:21) failed.\n");
		return -1;
	}
	printf("ftp.login(124.222.175.81:21) ok.\n");


	ftp.logout();

	return 0;
}