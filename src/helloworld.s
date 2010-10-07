	.file	"helloworld.c"
	.stabs	"helloworld.c",100,0,2,.Ltext0
	.section	".text"
.Ltext0:
	.stabs	"gcc2_compiled.",60,0,0,0
	.stabs	"int:t(0,1)=r(0,1);-2147483648;2147483647;",128,0,0,0
	.stabs	"char:t(0,2)=r(0,2);0;255;",128,0,0,0
	.stabs	"long int:t(0,3)=r(0,3);-2147483648;2147483647;",128,0,0,0
	.stabs	"unsigned int:t(0,4)=r(0,4);0;4294967295;",128,0,0,0
	.stabs	"long unsigned int:t(0,5)=r(0,5);0;4294967295;",128,0,0,0
	.stabs	"long long int:t(0,6)=r(0,6);-0;4294967295;",128,0,0,0
	.stabs	"long long unsigned int:t(0,7)=r(0,7);0;-1;",128,0,0,0
	.stabs	"short int:t(0,8)=r(0,8);-32768;32767;",128,0,0,0
	.stabs	"short unsigned int:t(0,9)=r(0,9);0;65535;",128,0,0,0
	.stabs	"signed char:t(0,10)=r(0,10);-128;127;",128,0,0,0
	.stabs	"unsigned char:t(0,11)=r(0,11);0;255;",128,0,0,0
	.stabs	"float:t(0,12)=r(0,1);4;0;",128,0,0,0
	.stabs	"double:t(0,13)=r(0,1);8;0;",128,0,0,0
	.stabs	"long double:t(0,14)=r(0,1);8;0;",128,0,0,0
	.stabs	"_Decimal32:t(0,15)=r(0,1);4;0;",128,0,0,0
	.stabs	"_Decimal64:t(0,16)=r(0,1);8;0;",128,0,0,0
	.stabs	"_Decimal128:t(0,17)=r(0,1);16;0;",128,0,0,0
	.stabs	"void:t(0,18)=(0,18)",128,0,0,0
	.stabs	"mytype:t(0,19)=(0,4)",128,0,0,0
	.globl globalvariable
	.section	.sdata,"aw",@progbits
	.align 2
	.type	globalvariable, @object
	.size	globalvariable, 4
globalvariable:
	.long	1234
	.section	.rodata
	.align 2
.LC0:
	.string	"halihello"
	.section	".text"
	.align 2
	.stabs	"hello2:F(0,18)",36,0,0,hello2
	.globl hello2
	.type	hello2, @function
hello2:
	.stabn	68,0,11,.LM0-.LFBB1
.LM0:
.LFBB1:
	stwu %r1,-16(%r1)
	mflr %r0
	stw %r31,12(%r1)
	stw %r0,20(%r1)
	mr %r31,%r1
	.stabn	68,0,12,.LM1-.LFBB1
.LM1:
	lis %r9,.LC0@ha
	la %r3,.LC0@l(%r9)
	bl puts
	.stabn	68,0,13,.LM2-.LFBB1
.LM2:
	lwz %r11,0(%r1)
	lwz %r0,4(%r11)
	mtlr %r0
	lwz %r31,-4(%r11)
	mr %r1,%r11
	blr
	.size	hello2, .-hello2
.Lscope1:
	.align 2
	.stabs	"hello:F(0,1)",36,0,0,hello
	.stabs	"a:p(0,1)",160,0,0,24
	.globl hello
	.type	hello, @function
hello:
	.stabn	68,0,16,.LM3-.LFBB2
.LM3:
.LFBB2:
	stwu %r1,-48(%r1)
	mflr %r0
	stw %r31,44(%r1)
	stw %r0,52(%r1)
	mr %r31,%r1
	stw %r3,24(%r31)
	.stabn	68,0,19,.LM4-.LFBB2
.LM4:
	lwz %r9,24(%r31)
	addi %r0,%r9,3
	stw %r0,24(%r31)
	.stabn	68,0,23,.LM5-.LFBB2
.LM5:
	bl hello2
	.stabn	68,0,25,.LM6-.LFBB2
.LM6:
	lwz %r0,24(%r31)
	.stabn	68,0,26,.LM7-.LFBB2
.LM7:
	mr %r3,%r0
	lwz %r11,0(%r1)
	lwz %r0,4(%r11)
	mtlr %r0
	lwz %r31,-4(%r11)
	mr %r1,%r11
	blr
	.size	hello, .-hello
	.stabs	"hello:(0,1)",128,0,0,8
	.stabn	192,0,0,.LFBB2-.LFBB2
	.stabn	224,0,0,.Lscope2-.LFBB2
.Lscope2:
	.section	.rodata
	.align 2
.LC2:
	.string	"Hello world!"
	.align 2
.LC3:
	.string	"a = %d\n"
	.align 2
.LC4:
	.string	"Goodbye world!"
	.section	".text"
	.align 2
	.stabs	"main:F(0,1)",36,0,0,main
	.globl main
	.type	main, @function
main:
	.stabn	68,0,30,.LM8-.LFBB3
.LM8:
.LFBB3:
	stwu %r1,-32(%r1)
	mflr %r0
	stw %r31,28(%r1)
	stw %r0,36(%r1)
	mr %r31,%r1
	.stabn	68,0,31,.LM9-.LFBB3
.LM9:
	li %r0,17
	stw %r0,16(%r31)
	.stabn	68,0,33,.LM10-.LFBB3
.LM10:
	lis %r9,.LC1@ha
	lfs %f0,.LC1@l(%r9)
	stfs %f0,8(%r31)
	.stabn	68,0,37,.LM11-.LFBB3
.LM11:
	lis %r9,.LC2@ha
	la %r3,.LC2@l(%r9)
	bl puts
	.stabn	68,0,39,.LM12-.LFBB3
.LM12:
	li %r3,5
	bl hello
	mr %r0,%r3
	stw %r0,20(%r31)
	.stabn	68,0,41,.LM13-.LFBB3
.LM13:
	lis %r9,globalvariable@ha
	li %r0,4321
	stw %r0,globalvariable@l(%r9)
	.stabn	68,0,43,.LM14-.LFBB3
.LM14:
	b .L6
.L7:
	.stabn	68,0,45,.LM15-.LFBB3
.LM15:
	lis %r9,.LC3@ha
	la %r3,.LC3@l(%r9)
	lwz %r4,20(%r31)
	crxor 6,6,6
	bl printf
	.stabn	68,0,43,.LM16-.LFBB3
.LM16:
	lwz %r9,20(%r31)
	addi %r0,%r9,1
	stw %r0,20(%r31)
.L6:
	lwz %r0,20(%r31)
	cmpwi %cr7,%r0,14
	ble %cr7,.L7
	.stabn	68,0,50,.LM17-.LFBB3
.LM17:
	lis %r9,.LC4@ha
	la %r3,.LC4@l(%r9)
	bl puts
	.stabn	68,0,52,.LM18-.LFBB3
.LM18:
	li %r0,0
	.stabn	68,0,53,.LM19-.LFBB3
.LM19:
	mr %r3,%r0
	lwz %r11,0(%r1)
	lwz %r0,4(%r11)
	mtlr %r0
	lwz %r31,-4(%r11)
	mr %r1,%r11
	blr
	.size	main, .-main
	.stabs	"a:(0,1)",128,0,0,20
	.stabs	"b:(0,1)",128,0,0,16
	.stabs	"charptr:(0,20)=*(0,2)",128,0,0,12
	.stabs	"hellofloat:(0,12)",128,0,0,8
	.stabn	192,0,0,.LFBB3-.LFBB3
	.stabn	224,0,0,.Lscope3-.LFBB3
.Lscope3:
	.stabs	"globalvariable:G(0,1)",32,0,0,0
	.section	.rodata
	.align 2
.LC1:
	.long	1050253722
	.section	".text"
	.stabs	"",100,0,0,.Letext0
.Letext0:
	.ident	"GCC: (GNU) 4.2.4 (adtools build 20090118)"
